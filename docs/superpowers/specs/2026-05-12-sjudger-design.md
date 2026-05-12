# SJudger Design

## Goal

在当前仓库内实现一个内置版 `sjudger` 执行核心，用它替换 [`src/runner/RunnerExecutionSupport.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/src/runner/RunnerExecutionSupport.cpp) 中对外部 `/usr/bin/sjudge` 和裸跑 fallback 的依赖。

这一轮设计只覆盖当前仓库真正需要的能力：

1. 执行 C++ 编译产物。
2. 执行 Python wrapper 脚本。
3. 支持 CPU 时间、真实时间、内存、栈、输出大小等基础限制。
4. 通过 `CMake option` 控制是否编译 `seccomp` 支持。

## Non-Goals

这一轮不做下面这些事情：

1. 不做运行时 `seccomp` 开关，不在 `config/config.json` 暴露相关配置。
2. 不复制 `Judger` 的全部 rule 体系，不实现 `general`、`golang`、`node` 等多规则分发。
3. 不要求 root，不引入 `setuid`/`setgid`/`setgroups` 作为首版前提。
4. 不新增独立 `sjudger` 可执行文件，首版只提供内置库 API。
5. 不改变 `CppRunner`、`PythonRunner` 的 prepare/compile 语义。
6. 不做与当前需求无关的大范围 runner 重构。

## Context

当前执行链路位于 [`src/runner/RunnerExecutionSupport.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/src/runner/RunnerExecutionSupport.cpp)：

1. 如果系统存在 `/usr/bin/sjudge`，就通过 [`sjudge/sjudge_call.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/sjudge/sjudge_call.cpp) 拼接命令并调用外部二进制。
2. 如果外部 `sjudge` 不存在，就退化到 `posix_spawn + waitpid + kill(SIGKILL)` 的裸跑逻辑。

这个实现有几个问题：

1. 行为依赖宿主机是否预装 `/usr/bin/sjudge`，构建产物不可移植。
2. 外部调用链可读性和可测性弱，错误路径只能靠输出文本反解析。
3. fallback 逻辑过于简单，资源限制与结果映射不统一。
4. `sjudge/` 目录名叫 `sjudge`，但实际上并没有本地评测核心。

仓库中同时存在参考实现 [Judger](/home/rainboy/mycode/boxtest-opencode-dev/Judger)。它的 `runner.c`、`child.c` 提供了可参考的核心思路：

1. 父进程负责 `fork`、真实时间监控、`wait4` 和结果采样。
2. 子进程负责 `setrlimit`、I/O 重定向、可选 `seccomp`、最后 `execve`。

这一轮采用“参考行为，不照搬源码结构”的方式重写一个精简的 C++ 版本。

## Design Principles

### KISS

首版只支持当前仓库已经需要的执行路径，不提前实现未来可能用到但当前没有调用方的特性。

### 模块职责单一

每个文件只负责一个问题：参数校验、子进程准备、父进程监控、结果映射、可选 `seccomp`。避免把所有逻辑重新揉进一个大文件。

### 接口稳定

对 runner 层暴露一个小而稳定的执行 API，避免 runner 直接理解 `fork`、`wait4`、`setrlimit` 等细节。

### 中文注释优先

新加的公共接口、关键分支和限制条件使用中文注释，重点解释“为什么这样做”和“这个模块负责什么”，而不是逐行复述代码。

## High-Level Architecture

建议把 `sjudge/` 重构为一个小型内置评测模块，目标结构如下：

1. `sjudge/JudgeTypes.h`
   放 `judge_config`、`judge_result`、结果枚举和公共常量。
2. `sjudge/SJudger.h`
   对外只暴露 `judge_result run_sjudger(const judge_config &config)`。
3. `sjudge/ConfigValidator.cpp`
   校验参数是否合法，统一返回配置错误。
4. `sjudge/ChildSetup.cpp`
   在子进程中负责工作目录切换、I/O 重定向、资源限制、可选 `seccomp` 和 `execve` 准备。
5. `sjudge/ParentMonitor.cpp`
   在父进程中负责 `fork` 后的等待、真实时间超时、`wait4`、`rusage` 采集。
6. `sjudge/ResultMapper.cpp`
   把 `wait4` 的状态和采样结果映射成 `judge_result.result`。
7. `sjudge/SeccompPolicy.h`
   声明可选 `seccomp` 装载接口。
8. `sjudge/SeccompPolicy.cpp`
   当 `SJUDGER_ENABLE_SECCOMP=ON` 时提供真实实现。
9. `sjudge/SeccompPolicyStub.cpp`
   当 `SJUDGER_ENABLE_SECCOMP=OFF` 时提供空实现。

对 runner 层来说，只需要知道两件事：

1. 组装 `judge_config`。
2. 调用 `run_sjudger()` 并映射返回值。

## Data Model

### `judge_config`

首版保留和当前调用方匹配的字段，并适度补齐执行所需信息：

1. `max_cpu_time_ms`
2. `max_real_time_ms`
3. `max_memory_bytes`
4. `max_stack_bytes`
5. `max_output_bytes`
6. `cwd`
7. `exe_path`
8. `input_path`
9. `output_path`
10. `error_path`
11. `log_path`
12. `args`
13. `env`

首版不包含 `uid`、`gid`、`seccomp_rule_name` 这样的运行时分发字段，因为：

1. 用户明确要求 `seccomp` 只做编译期开关。
2. 当前仓库调用方没有按语言切换 seccomp 规则的需求。
3. 不引入这些字段可以保持 API 简单。

### `judge_result`

保留当前 runner 已经依赖的结果字段：

1. `cpu_time_ms`
2. `real_time_ms`
3. `memory_bytes`
4. `signal`
5. `exit_code`
6. `error`
7. `result`

`result` 继续使用当前仓库已有的评测结果枚举语义，例如 `SUCCESS`、`CPU_TIME_LIMIT_EXCEEDED`、`REAL_TIME_LIMIT_EXCEEDED`、`MEMORY_LIMIT_EXCEEDED`、`RUNTIME_ERROR`、`SYSTEM_ERROR`。

## Execution Flow

### 1. Runner 侧组装配置

[`src/runner/RunnerExecutionSupport.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/src/runner/RunnerExecutionSupport.cpp) 的 `run_executable_case()` 负责把 `RunnerCaseInput` 和执行文件路径转换成 `judge_config`：

1. `exe_path` 使用编译产物或 Python wrapper。
2. `cwd` 使用可执行文件所在目录。
3. `input_path`、`output_path` 来自单点测试。
4. 时间和内存限制沿用 `RunnerCaseInput`。

这一步只做字段转换，不包含任何进程控制逻辑。

### 2. `run_sjudger()` 作为总控入口

`run_sjudger()` 负责：

1. 初始化默认结果。
2. 调用配置校验。
3. 记录起始时间。
4. `fork()`。
5. 子进程调用 `ChildSetup`。
6. 父进程调用 `ParentMonitor`。
7. 调用 `ResultMapper` 得到统一 verdict。

### 3. 子进程准备

`ChildSetup` 按下面顺序执行：

1. 如有 `cwd`，先 `chdir`。
2. 打开并重定向 `stdin`、`stdout`、`stderr`。
3. 应用 `RLIMIT_STACK`。
4. 应用 `RLIMIT_AS` 作为内存上限。
5. 应用 `RLIMIT_CPU`。
6. 应用 `RLIMIT_FSIZE` 作为输出大小上限。
7. 如编译时启用了 `seccomp`，调用 `SeccompPolicy` 装载默认策略。
8. 调用 `execve()`。

如果任一步失败，子进程应使用明确的内部错误码退出，让父进程可以把它识别成 `SYSTEM_ERROR`。

### 4. 父进程监控

`ParentMonitor` 负责：

1. 持有子进程 PID。
2. 使用 `wait4()` 或 `waitpid(WNOHANG)` 轮询等待。
3. 自己计算真实时间，必要时超时后 `kill(SIGKILL)`。
4. 收集 `rusage`。
5. 返回完整的原始状态：`wait status`、`signal`、`exit code`、`cpu time`、`real time`、`memory`。

真实时间检查放在父进程，不依赖 `RLIMIT_CPU`，这样可以覆盖：

1. Busy loop。
2. 阻塞等待。
3. Python wrapper 这类解释器脚本。

### 5. 结果映射

`ResultMapper` 负责把原始状态映射为稳定 verdict：

1. 正常退出且 `exit_code == 0`，先判为 `SUCCESS`。
2. `real_time > limit`，映射成 `REAL_TIME_LIMIT_EXCEEDED`。
3. `cpu_time > limit`，映射成 `CPU_TIME_LIMIT_EXCEEDED`。
4. `memory > limit`，映射成 `MEMORY_LIMIT_EXCEEDED`。
5. 非零退出码或收到异常信号，映射成 `RUNTIME_ERROR`。
6. `fork`、`dup2`、`setrlimit`、`execve` 等基础设施失败，映射成 `SYSTEM_ERROR`。

映射顺序要固定，避免同一状态因为判断顺序不同产生不稳定结果。

## Seccomp Strategy

### Build-Time Option

根 `CMakeLists.txt` 引入：

```cmake
option(SJUDGER_ENABLE_SECCOMP "Build sjudger with seccomp support" ON)
```

构建规则如下：

1. `ON`
   - 编译 `sjudge/SeccompPolicy.cpp`
   - 查找并链接 `libseccomp`
   - 定义 `SJUDGER_ENABLE_SECCOMP=1`
2. `OFF`
   - 编译 `sjudge/SeccompPolicyStub.cpp`
   - 不链接 `libseccomp`
   - 不暴露运行时开关

### Runtime Behavior

开启 `SJUDGER_ENABLE_SECCOMP` 后，所有评测都统一加载默认策略。

关闭 `SJUDGER_ENABLE_SECCOMP` 后，所有评测都裸跑。

首版不支持运行时切换，不支持按语言或 rule name 分发策略。

### Policy Scope

首版 seccomp 策略只服务当前仓库已支持的执行方式：C++ 可执行文件和 Python wrapper。

设计上采用“一个默认白名单策略”，而不是移植 `Judger` 的多规则表。原因是：

1. 当前调用方只有一种统一执行入口。
2. 用户要求 KISS。
3. 多规则分发会把复杂度引到还没有需求的地方。

如果后续需要按语言拆分策略，再在这个模块内部扩展，而不是提前暴露复杂 API。

## Integration Plan

### `sjudge/` 目录

保留目录名 `sjudge/`，但把内容从“外部命令调用层”升级为“真正的内置评测核心”。

[`sjudge/sjudge_call.h`](/home/rainboy/mycode/boxtest-opencode-dev/sjudge/sjudge_call.h) 和 [`sjudge/sjudge_call.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/sjudge/sjudge_call.cpp) 可以通过两种方式演进：

1. 直接删除，改为新的 `SJudger.h/.cpp`。
2. 保留文件名，但让它们内部转调 `run_sjudger()`。

推荐第 2 种，原因是当前 runner 已经 include 了 `sjudge_call.h`，这样可以先稳定接口，再逐步重命名内部实现，避免无意义改动扩散。

### Runner 层

[`src/runner/RunnerExecutionSupport.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/src/runner/RunnerExecutionSupport.cpp) 做以下调整：

1. 删除对 `/usr/bin/sjudge` 的存在性分支。
2. 删除 `posix_spawn` 裸跑 fallback。
3. 统一通过内置 `run_sjudger()` 执行。
4. 保留输出比较和 `judge_result` 到 `SubmissionVerdict` 的映射。

[`src/runner/CppRunner.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/src/runner/CppRunner.cpp) 与 [`src/runner/PythonRunner.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/src/runner/PythonRunner.cpp) 不需要理解评测实现细节，只继续准备好可执行目标和测试文件。

### Build System

`bt_runner` 继续承载 runner 与 sjudger 代码，但显式列出新增源文件。

如果 `SJUDGER_ENABLE_SECCOMP=ON`，则只给 `bt_runner` 增加 seccomp 依赖，不把依赖散到其他模块。

## Testing Strategy

### SJudger Unit Coverage

增加新的 `sjudge`/runner 级回归测试，至少覆盖：

1. 配置非法时返回 `INVALID_CONFIG` 或对应系统错误。
2. 普通 echo 程序返回 AC。
3. 非零退出码映射为 RE。
4. 死循环映射为 TLE。
5. 超大内存申请映射为 MLE。

这些测试应尽量走公开接口，不直接依赖内部私有函数。

### Existing Runner Regression

保留并运行现有测试：

1. [`test/test_cpp_runner.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/test/test_cpp_runner.cpp)
2. [`test/test_python_runner.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/test/test_python_runner.cpp)
3. [`test/test_runner_factory.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/test/test_runner_factory.cpp)
4. [`test/test_submission_service.cpp`](/home/rainboy/mycode/boxtest-opencode-dev/test/test_submission_service.cpp)

原因是执行核心替换后，最容易回归的地方就是：

1. 单点执行结果映射。
2. Python wrapper 行为。
3. 提交服务对 case result 的汇总。

### Conditional Seccomp Tests

当 `SJUDGER_ENABLE_SECCOMP=ON` 时，增加条件测试：

1. 构造一个触发受限系统调用的小程序。
2. 断言其结果不是 AC，且不是基础设施错误。

当 `SJUDGER_ENABLE_SECCOMP=OFF` 时：

1. 不编译这类测试，或者注册为跳过。

### Full Validation

实现完成后，至少跑：

1. `ctest --test-dir build --output-on-failure`
2. 如有针对 sjudger 的单独测试目标，逐个点跑失败场景和通过场景

## Expected Behavioral Changes

这次改动会带来几个有意为之的行为变化：

1. 不再依赖宿主机预装 `/usr/bin/sjudge`。
2. 不再保留当前“外部 sjudge / 裸跑 fallback”两套行为。
3. 统一由同一套资源限制和结果映射逻辑处理 C++ 与 Python。

这些变化的目标是提升一致性，而不是完全复刻当前分支行为。

## Deliberate Differences From `Judger`

虽然设计参考了 `Judger`，但首版明确不照搬以下内容：

1. 不要求 root。
2. 不实现 `uid/gid` 切换。
3. 不实现多 seccomp rule name 分发。
4. 不照搬它的 C 文件组织方式。

这样做的原因是：

1. 当前仓库本地测试和开发流程并不是围绕 root 组织的。
2. 用户要求 KISS、模块化、人类易懂。
3. 当前调用方并不需要完整的历史能力集合。

## Risks and Mitigations

### 风险 1：`RLIMIT_AS` 在不同程序上的表现不完全一致

缓解方式：

1. 保持结果映射以实际 `wait` 状态和采样值为准。
2. 对内存超限测试使用稳定、明显的超限样例。

### 风险 2：Python 解释器在 seccomp 下可能比原生 C++ 需要更多系统调用

缓解方式：

1. 首版默认策略设计时必须用 Python wrapper 场景验证。
2. 如果默认策略过严，应优先调整默认白名单，而不是引入运行时多策略。

### 风险 3：把旧 fallback 一次性删掉会扩大首轮改动影响

缓解方式：

1. 先通过测试锁定 runner 外部行为。
2. 新实现先在公开接口层兼容旧 `judge_result` 结构。
3. 保持 `CppRunner`、`PythonRunner` 代码改动最小。

## Acceptance Criteria

当下面条件全部满足时，这一轮设计算落地成功：

1. `run_executable_case()` 不再依赖 `/usr/bin/sjudge` 或 `posix_spawn` fallback。
2. 工程内存在单一的内置 sjudger 执行核心。
3. `SJUDGER_ENABLE_SECCOMP` 可以控制是否编译 seccomp 支持。
4. 关闭 seccomp 时工程仍可构建并通过相关测试。
5. 现有 C++/Python runner 回归测试通过。
6. 新增代码符合模块化拆分，并带有中文注释。
