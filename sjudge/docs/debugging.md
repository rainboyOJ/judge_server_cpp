# sjudger 调试手册

这篇文档用于排查 sjudger 执行异常。

建议先理解这几个字段：

| 字段 | 含义 |
| --- | --- |
| `result.result` | sjudger 结果码 |
| `result.error` | 基础设施错误码 |
| `result.exit_code` | 用户程序正常退出时的返回码 |
| `result.signal` | 用户程序被信号终止时的信号 |
| `result.real_time_ms` | 真实时间 |
| `result.cpu_time_ms` | CPU 时间 |
| `result.memory_bytes` | 内存峰值 |

## 先跑哪些测试

常用命令：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_sjudger test_cpp_runner test_python_runner test_submission_service -j
ctest --test-dir build -R '^(test_sjudger|test_cpp_runner|test_python_runner|test_submission_service)$' --output-on-failure
```

如果只改 sjudger：

```bash
cmake --build build --target test_sjudger -j
ctest --test-dir build -R '^test_sjudger$' --output-on-failure
```

如果改 runner 接入：

```bash
cmake --build build --target test_cpp_runner test_python_runner -j
ctest --test-dir build -R '^(test_cpp_runner|test_python_runner)$' --output-on-failure
```

## 如何判断是 judger 错还是用户程序错

先看 `result.error`。

如果 `result.error != 0`，通常是 judger 基础设施错误。

如果 `result.error == 0`，再看：

- `result.result`
- `exit_code`
- `signal`
- 时间和内存数据

## 常见错误码

### INVALID_CONFIG

含义：配置非法。

常见原因：

- `exe_path` 为空。
- 后续如果扩展校验，也可能是路径或参数非法。

排查：

- 检查 runner 是否正确设置 `config.exe_path`。
- 检查执行文件是否存在。

### DUP2_FAILED

含义：输入输出重定向失败。

常见原因：

- `input_path` 不存在。
- `output_path` 所在目录不存在。
- 没有文件权限。
- stderr 路径无法创建。

排查：

```bash
ls -l <input_path>
ls -ld <output_dir>
```

对应代码：

```text
sjudge/ChildSetup.cpp
```

### SETRLIMIT_FAILED

含义：资源限制设置失败。

常见原因：

- 设置了非法或系统不允许的限制。
- 当前用户无权提高 hard limit。

排查：

```bash
ulimit -a
```

检查 `max_*` 字段是否异常。

### EXECVE_FAILED

含义：执行用户程序失败。

常见原因：

- 文件不存在。
- 没有执行权限。
- 脚本 shebang 不对。
- 动态链接器路径不存在。

排查：

```bash
ls -l <exe_path>
file <exe_path>
ldd <exe_path>
```

如果是脚本：

```bash
head -1 <script>
which python3
which sh
```

### WAIT_FAILED

含义：父进程等待子进程失败。

常见原因较少，通常需要看 errno。当前代码没有把 errno 透出，后续可以增强日志。

## TLE 排查

TLE 可能来自：

- `max_real_time_ms`
- `max_cpu_time_ms`

先看 `real_time_ms` 和 `cpu_time_ms`。

如果真实时间很高但 CPU 时间很低，可能是：

- `sleep`
- 阻塞 IO
- 等待锁或子进程

如果 CPU 时间很高，通常是：

- 死循环
- 高计算量

复现 demo：

```bash
cd sjudge/demo_code_for_tutorial
make
./04_realtime_limit
./06_mini_judger tle
```

## MLE 排查

MLE 可能来自：

- `memory_bytes > max_memory_bytes`
- 内存限制下收到 `SIGSEGV/SIGABRT/SIGKILL`
- 内存限制下解释器启动失败并返回 `127`

需要注意：`RLIMIT_AS` 限制地址空间，`ru_maxrss` 是 RSS 峰值，两者不是同一个概念。

所以有时程序被内存限制影响，但 `memory_bytes` 没有超过上限。

复现 demo：

```bash
cd sjudge/demo_code_for_tutorial
make
./05_rlimit_memory
./06_mini_judger mle
```

## RE 排查

RE 通常来自：

- 非零 exit code。
- 非超时、非 MLE 的异常 signal。

排查：

```bash
echo $?
```

或者直接运行用户程序：

```bash
./solution < case.in > out.txt
```

如果是 Python：

```bash
python3 solution.py < case.in > out.txt
```

## 输出错误不是 sjudger 的事

sjudger 只负责执行和资源状态。

输出比较在 runner 层：

```text
src/runner/RunnerExecutionSupport.cpp
```

所以：

- sjudger `SUCCESS`
- runner 最终 verdict 仍可能是 WA

这是正常的。

## 使用 strace

如果怀疑系统调用、动态库、脚本解释器路径有问题，可以用 `strace`：

```bash
strace -f -o trace.log ./solution < case.in > out.txt
```

查看最后失败位置：

```bash
tail -80 trace.log
```

查看 syscall 列表：

```bash
awk '{print $2}' trace.log | sed 's/(.*//' | sort -u
```

seccomp 调试时，`strace` 非常重要。

## 增加新测试时的建议

每个异常路径最好有一个小测试：

- 非法配置。
- 输入文件缺失。
- 可执行文件缺失。
- 非零退出码。
- 死循环。
- 内存申请过大。
- 输出过大。

测试应尽量使用临时目录，避免污染仓库。

本仓库 `test/test_sjudger.cpp` 已经有可参考的临时目录 helper。

