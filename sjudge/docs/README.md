# sjudger 文档

这个目录记录 `sjudge/` 内置评测核心的设计和实现。

建议阅读顺序：

1. [`tutorial.md`](./tutorial.md)
   - 教学型文档。
   - 从“为什么 judger 要这样设计”讲到 `fork/execve/wait4/setrlimit` 的基本原理。
   - 看完后应该能理解本仓库 sjudger 的架构，并能自己写一个最小 judger。
2. [`from-scratch-mini-judger.md`](./from-scratch-mini-judger.md)
   - 动手教学。
   - 按步骤从 `fork/execve` 一直写到一个最小 judger。
   - 对应 demo 在 `../demo_code_for_tutorial/`。
3. [`result-mapping.md`](./result-mapping.md)
   - 判题结果语义。
   - 专门解释 AC/TLE/MLE/RE/SYSTEM_ERROR 如何判断。
4. [`seccomp.md`](./seccomp.md)
   - seccomp 教学文档。
   - 解释系统调用过滤的原理、libseccomp 的基本用法，以及 sjudger 后续如何接入。
5. [`security-boundary.md`](./security-boundary.md)
   - 安全边界。
   - 说明当前 sjudger 做了什么、没做什么，以及生产级沙箱还需要什么。
6. [`debugging.md`](./debugging.md)
   - 调试手册。
   - 帮助定位执行失败、资源超限、结果映射异常。
7. 本文件后续章节
   - 维护型速查。
   - 用来快速定位模块、字段、测试命令和当前限制。

## 当前目标

`sjudge/` 目录现在承载仓库内置的 sjudger 执行核心。它的目标是替代 runner 层原先对外部 `/usr/bin/sjudge` 和本地裸跑 fallback 的依赖，让 C++、Python 等 runner 都走同一套进程控制、资源限制和结果映射逻辑。

当前对外稳定入口是：

```cpp
judge_result run_sjudger(const judge_config &config);
```

入口声明保留在 `sjudge/sjudge_call.h`，这样现有 runner 不需要感知 sjudger 内部拆分。

## 模块结构

- `sjudge_call.h/.cpp`
  - 定义 `judge_config`、`judge_result`、结果码枚举。
  - 提供 `run_sjudger()` 入口。
  - 仍保留旧的 `call_sjudge()`，用于兼容历史外部 sjudge 调用代码。
- `ConfigValidator`
  - 校验执行配置。
  - 当前主要检查 `exe_path` 是否有效。
- `ChildSetup`
  - 在子进程中切换工作目录。
  - 重定向 `stdin/stdout/stderr`。
  - 应用资源限制。
  - 最后调用 `execve()`。
- `ParentMonitor`
  - 在父进程中等待子进程。
  - 统计真实时间、CPU 时间和内存峰值。
  - 真实时间超限时主动 `SIGKILL` 子进程。
- `ResultMapper`
  - 把 `ParentMonitor` 采集到的退出状态映射成 `SUCCESS/TLE/MLE/RE` 等统一结果码。
- `SeccompPolicy`
  - 当前接入的是 stub。
  - 后续可以在 `SJUDGER_ENABLE_SECCOMP` 打开时替换成真正的 seccomp 策略实现。

## 执行流程

1. runner 调用 `run_executable_case()`。
2. `run_executable_case()` 把 `RunnerCaseInput` 转换成 `judge_config`。
3. `run_sjudger()` 校验配置。
4. 父进程 `fork()`。
5. 子进程执行 `ChildSetup`：
   - `chdir(config.cwd)`
   - 打开并重定向输入、输出、错误输出文件
   - 应用资源限制
   - `execve(config.exe_path, argv, environ)`
6. 父进程执行 `ParentMonitor`：
   - 轮询 `wait4(WNOHANG)`
   - 统计真实时间
   - 超过 `max_real_time_ms` 后杀死子进程
   - 收集 `rusage`
7. `ResultMapper` 生成最终 `judge_result.result`。
8. runner 把 sjudger 结果码转换成 `SubmissionVerdict`。
9. 如果执行结果是 `SUCCESS`，runner 再比较用户输出和标准输出。

## judge_config 字段

| 字段 | 含义 | `0` 的语义 |
| --- | --- | --- |
| `max_cpu_time_ms` | CPU 时间限制，毫秒 | 不限制 |
| `max_real_time_ms` | 真实时间限制，毫秒 | 不限制 |
| `max_memory_bytes` | 地址空间限制，字节 | 不限制 |
| `max_stack_bytes` | 栈大小限制，字节 | 不限制 |
| `max_output_bytes` | 输出文件大小限制，字节 | 不限制 |
| `cwd` | 子进程工作目录 | 不切换目录 |
| `exe_path` | 要执行的程序路径 | 非法配置 |
| `input_path` | 标准输入来源 | 默认 `/dev/stdin` |
| `output_path` | 标准输出目标 | 默认 `/dev/stdout` |
| `error_path` | 标准错误目标 | 默认 `/dev/stderr` |
| `args` | `execve()` 参数 | 为空时使用 `exe_path` 作为 argv[0] |
| `env` | 预留环境变量字段 | 当前暂未使用 |

## 资源限制

当前资源限制在子进程中通过 `setrlimit()` 应用：

- `RLIMIT_STACK`
  - 来源：`max_stack_bytes`
- `RLIMIT_AS`
  - 来源：`max_memory_bytes`
  - 用来限制进程地址空间。
- `RLIMIT_FSIZE`
  - 来源：`max_output_bytes`
  - 用来限制输出文件大小。
- `RLIMIT_CPU`
  - 来源：`max_cpu_time_ms`
  - 内核接口以秒为单位，因此毫秒会向上取整到秒。

真实时间限制不依赖 `setrlimit()`，由父进程用单调时钟统计。这样可以覆盖阻塞等待、死循环以及解释器 wrapper 等场景。

## 结果映射

`ResultMapper` 当前按固定顺序判断：

1. 父进程真实时间超时，返回 `REAL_TIME_LIMIT_EXCEEDED`。
2. 内存峰值超过 `max_memory_bytes`，返回 `MEMORY_LIMIT_EXCEEDED`。
3. 在设置了内存限制时，常见内存失败信号也返回 `MEMORY_LIMIT_EXCEEDED`。
4. 在设置了内存限制时，`exit_code == 127` 也按 `MEMORY_LIMIT_EXCEEDED` 处理。这覆盖解释器在极低地址空间限制下启动失败的情况。
5. CPU 时间超过 `max_cpu_time_ms`，返回 `CPU_TIME_LIMIT_EXCEEDED`。
6. 非零退出码或异常信号，返回 `RUNTIME_ERROR`。
7. 其他情况返回 `SUCCESS`。

子进程准备阶段的基础设施错误不会走普通 RE 映射。例如重定向失败、`execve()` 失败、`setrlimit()` 失败会被 `run_sjudger()` 转换成 `SYSTEM_ERROR`，并把具体错误码写入 `judge_result.error`。

## runner 集成

当前 `src/runner/RunnerExecutionSupport.cpp` 已统一使用内置 sjudger：

- 不再检查 `/usr/bin/sjudge`。
- 不再使用 `posix_spawn` fallback。
- C++ runner 直接执行编译产物。
- Python runner 先生成 `run_python.sh` wrapper，再交给同一执行入口。

runner 侧的结果映射如下：

| sjudger 结果 | SubmissionVerdict |
| --- | --- |
| `SUCCESS` | `AC`，随后继续比较输出 |
| `CPU_TIME_LIMIT_EXCEEDED` | `TLE` |
| `REAL_TIME_LIMIT_EXCEEDED` | `TLE` |
| `MEMORY_LIMIT_EXCEEDED` | `MLE` |
| `RUNTIME_ERROR` | `RE` |
| `SYSTEM_ERROR` | `SYSTEM_ERROR` |

## 构建开关

根 `CMakeLists.txt` 暴露：

```cmake
option(SJUDGER_ENABLE_SECCOMP "Build sjudger with seccomp support" ON)
```

当前实现仍编译 `SeccompPolicyStub.cpp`。真正的 `SeccompPolicy.cpp` 和 libseccomp 链接规则还没有完成，因此这个开关目前只保留接口形状，不提供实际系统调用过滤。

## 测试

常用验证命令：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_sjudger test_cpp_runner test_python_runner test_submission_service -j
ctest --test-dir build -R '^(test_sjudger|test_cpp_runner|test_python_runner|test_submission_service)$' --output-on-failure
```

相关测试覆盖：

- `test_sjudger`
  - 非法配置。
  - 成功执行并写输出。
  - 子进程准备失败。
  - 真实时间超限。
  - 非零退出码映射为 RE。
- `test_cpp_runner`
  - C++ 编译与执行。
  - 非零退出码 RE。
  - 不依赖外部 `/usr/bin/sjudge` 的 TLE。
- `test_python_runner`
  - Python 语法检查。
  - Python 异常 RE。
  - Python 死循环 TLE。
  - Python 内存限制 MLE。

## 当前限制

- `config.env` 字段暂未传给 `execve()`，当前仍使用父进程 `environ`。
- seccomp 还是 stub，没有真正限制系统调用。
- `RLIMIT_CPU` 只能按秒限制，毫秒级 CPU 限制依赖结果映射兜底。
- 当前没有 uid/gid 降权，也没有独立 mount namespace、cgroup 或 chroot。
- 输出比较仍在 runner 层完成，sjudger 只负责执行和资源结果。
