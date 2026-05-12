# 判题结果映射：从进程状态到 AC/TLE/MLE/RE

judger 最容易写错的地方不是 `fork()`，而是结果映射。

底层进程只告诉你：

- 程序是否正常退出。
- exit code 是多少。
- 是否被 signal 杀死。
- 用了多少 CPU 时间。
- 内存峰值是多少。
- 父进程是否主动杀了它。

但用户和服务层需要的是：

- AC
- TLE
- MLE
- RE
- SYSTEM_ERROR

这中间必须有一层稳定的语义转换。本仓库对应模块是：

```text
sjudge/ResultMapper.cpp
```

## 两类错误必须先分开

第一类：用户程序错误。

例如：

- 用户程序 `return 1`
- 用户程序抛异常
- 用户程序访问非法内存
- 用户程序超时
- 用户程序超内存

这些应该映射成 TLE、MLE、RE。

第二类：judger 基础设施错误。

例如：

- 输入文件打不开。
- 输出文件无法创建。
- `setrlimit()` 失败。
- `execve()` 失败。
- `fork()` 失败。
- `wait4()` 失败。

这些不应该报 RE，因为不是用户程序逻辑导致的。它们应该报 SYSTEM_ERROR，并记录具体错误码。

本仓库在 `sjudge_call.cpp` 中先识别子进程 setup 错误，再调用 `ResultMapper` 做普通结果映射。

## 为什么判断顺序重要

同一个底层状态可能有多个解释。

例如：真实时间超时后，父进程会发送 `SIGKILL`。

底层状态看起来是：

```text
signal = SIGKILL
timed_out = true
```

如果先判断 `signal != 0`，就会报 RE。

但正确语义是：父进程因为真实时间超限杀掉它，所以应该报 TLE。

因此 `timed_out` 必须先判断。

## 当前映射顺序

当前 `ResultMapper` 的顺序是：

1. `monitor.timed_out`
   - 返回 `REAL_TIME_LIMIT_EXCEEDED`
2. `memory_bytes > max_memory_bytes`
   - 返回 `MEMORY_LIMIT_EXCEEDED`
3. 设置了内存限制，并且 signal 是常见内存失败信号
   - 返回 `MEMORY_LIMIT_EXCEEDED`
4. 设置了内存限制，并且 `exit_code == 127`
   - 返回 `MEMORY_LIMIT_EXCEEDED`
5. `cpu_time_ms > max_cpu_time_ms`
   - 返回 `CPU_TIME_LIMIT_EXCEEDED`
6. `signal != 0 || exit_code != 0`
   - 返回 `RUNTIME_ERROR`
7. 其他
   - 返回 `SUCCESS`

## SUCCESS 和 AC 不是一回事

在 sjudger 层：

```text
SUCCESS = 程序成功运行结束
```

在 runner 层：

```text
AC = 程序运行成功，并且输出和标准答案一致
```

所以 `run_sjudger()` 返回 `SUCCESS` 后，runner 还要比较输出。

如果输出不一致，最终 verdict 是 WA，而不是 SUCCESS。

## TLE

TLE 有两类来源：

1. 真实时间超限。
2. CPU 时间超限。

真实时间由父进程主动监控：

```text
real_time_ms > max_real_time_ms
```

CPU 时间来自 `wait4()` 的 `rusage`：

```text
cpu_time_ms > max_cpu_time_ms
```

本仓库 runner 层把下面两个 sjudger 结果都映射成 `SubmissionVerdict::TLE`：

- `CPU_TIME_LIMIT_EXCEEDED`
- `REAL_TIME_LIMIT_EXCEEDED`

## MLE

MLE 不能只看 `ru_maxrss`。

原因：

- `RLIMIT_AS` 限制的是地址空间。
- 程序可能申请失败后自己退出。
- 解释器可能在启动阶段失败。
- 内存失败不一定让 RSS 超过上限。

当前映射做了三个兜底：

1. RSS 峰值超过限制。
2. 内存限制下出现 `SIGSEGV/SIGABRT/SIGKILL`。
3. 内存限制下 `exit_code == 127`。

第 3 条主要覆盖 Python wrapper 在极低内存限制下启动失败的情况。

注意：这是工程折中，不是数学上完美的判断。更严谨的 MLE 判断通常需要 cgroup 或更强的运行时采样。

## RE

RE 表示用户程序运行时错误。

典型条件：

```text
signal != 0 || exit_code != 0
```

但它必须排在 TLE/MLE 后面。否则超时和内存错误可能被误判成 RE。

## SYSTEM_ERROR

SYSTEM_ERROR 表示 judger 自己没有成功完成执行任务。

常见错误码：

| 错误码 | 含义 |
| --- | --- |
| `INVALID_CONFIG` | 配置非法 |
| `FORK_FAILED` | `fork()` 失败 |
| `WAIT_FAILED` | `wait4()` 失败 |
| `DUP2_FAILED` | 输入输出重定向失败 |
| `SETRLIMIT_FAILED` | 资源限制设置失败 |
| `EXECVE_FAILED` | 执行用户程序失败 |
| `LOAD_SECCOMP_FAILED` | seccomp 策略加载失败 |

这些错误要优先暴露给维护者，因为它们通常表示评测环境或 judger 代码有问题。

## 判断结果时推荐问的几个问题

排查一个异常 verdict 时，按这个顺序问：

1. `monitor.error` 是否非 0？
2. 子进程 exit code 是否是 setup 错误码？
3. `timed_out` 是否为 true？
4. 是否设置了内存限制？
5. `memory_bytes` 是否超过限制？
6. `signal` 是多少？
7. `exit_code` 是多少？
8. CPU 时间是否超过限制？
9. 输出是否比较失败？

这个顺序基本对应当前代码路径。

