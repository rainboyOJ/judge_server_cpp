# 从零写一个最小 judger

这篇文档把 `tutorial.md` 里的原理变成可运行练习。目标不是写出生产级沙箱，而是用最少代码理解 judger 的核心骨架。

对应 demo 目录：

```text
sjudge/demo_code_for_tutorial/
```

编译：

```bash
cd sjudge/demo_code_for_tutorial
make
```

## 第 1 步：fork + execve

对应代码：

```text
01_fork_exec.cpp
```

这个 demo 只做一件事：创建子进程，并在子进程里执行 `/bin/echo`。

核心逻辑：

```cpp
const pid_t pid = fork();
if (pid == 0) {
    char *const argv[] = {
        const_cast<char *>("/bin/echo"),
        const_cast<char *>("hello from child"),
        nullptr,
    };
    execve("/bin/echo", argv, environ);
    _exit(127);
}
waitpid(pid, &status, 0);
```

你需要理解：

- `fork()` 后有父子两个进程。
- 子进程 `execve()` 成功后不再执行原来的 C++ 代码。
- `execve()` 返回就表示失败。
- 父进程必须等待子进程，否则会产生僵尸进程。

运行：

```bash
./01_fork_exec
```

## 第 2 步：重定向输入输出

对应代码：

```text
02_redirect_io.cpp
```

真实 judger 不会让用户程序直接读终端。它会把测试输入文件接到 stdin，把用户输出写到文件。

核心操作：

```cpp
int in = open("case.in", O_RDONLY);
int out = open("case.out", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(in, STDIN_FILENO);
dup2(out, STDOUT_FILENO);
```

这个 demo 用 `/bin/cat` 模拟用户程序：

```text
case.in -> stdin -> /bin/cat -> stdout -> case.out
```

运行：

```bash
./02_redirect_io
cat demo_tmp/case.out
```

你需要理解：

- `dup2()` 改变的是文件描述符指向。
- 用户程序不需要知道输入输出文件路径。
- 如果 `open()` 或 `dup2()` 失败，这是 judger 基础设施错误，不是用户程序 RE。

## 第 3 步：wait4 采集状态和资源

对应代码：

```text
03_wait4_usage.cpp
```

`waitpid()` 只能等待退出状态；`wait4()` 还能拿到 `rusage`。

核心逻辑：

```cpp
rusage usage{};
wait4(pid, &status, 0, &usage);
```

然后解析：

```cpp
WIFEXITED(status)
WEXITSTATUS(status)
WIFSIGNALED(status)
WTERMSIG(status)
usage.ru_utime
usage.ru_maxrss
```

运行：

```bash
./03_wait4_usage
```

你需要理解：

- exit code 是程序主动返回的状态。
- signal 是程序被外部或内核终止的原因。
- `ru_maxrss` 是内存峰值，Linux 上通常以 KB 为单位。

## 第 4 步：真实时间限制

对应代码：

```text
04_realtime_limit.cpp
```

CPU 时间限制不能处理 `sleep()` 或阻塞等待，所以 judger 必须自己统计真实时间。

核心逻辑：

```cpp
while (true) {
    pid_t rc = waitpid(pid, &status, WNOHANG);
    if (rc == pid) {
        break;
    }

    if (elapsed_ms > limit_ms) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        break;
    }
}
```

这个 demo 运行 `/bin/sleep 10`，但父进程在 500ms 左右杀掉它。

运行：

```bash
./04_realtime_limit
```

你需要理解：

- `WNOHANG` 让父进程非阻塞轮询。
- TLE 不是用户程序自己返回的，而是 judger 主动终止的。
- 被超时杀死的程序通常会带有 `SIGKILL`。

## 第 5 步：内存限制

对应代码：

```text
05_rlimit_memory.cpp
```

这个 demo 在子进程中设置 `RLIMIT_AS`，然后运行一个会申请大量内存的程序。

核心逻辑：

```cpp
rlimit limit{};
limit.rlim_cur = memory_bytes;
limit.rlim_max = memory_bytes;
setrlimit(RLIMIT_AS, &limit);
```

运行：

```bash
./05_rlimit_memory
```

你需要理解：

- `RLIMIT_AS` 限制地址空间，不是简单的物理内存。
- 程序可能抛异常、返回非零、收到 signal，或者在运行时启动阶段失败。
- MLE 映射通常需要结合内存峰值、signal、exit code。

## 第 6 步：组合成最小 judger

对应代码：

```text
06_mini_judger.cpp
```

这个 demo 把前几步合并起来：

1. 配置可执行文件、输入、输出、时间限制、内存限制。
2. `fork()`。
3. 子进程重定向 IO、设置资源限制、`execve()`。
4. 父进程 `wait4(WNOHANG)` 监控真实时间。
5. 采集 exit code、signal、CPU 时间、内存。
6. 映射成简单 verdict。

运行：

```bash
./06_mini_judger ok
./06_mini_judger re
./06_mini_judger tle
./06_mini_judger mle
```

输出会类似：

```text
verdict=AC exit_code=0 signal=0 real_time_ms=...
```

你需要理解：

- 一个 judger 的核心不是某一个 API，而是一条完整控制链路。
- 子进程只负责准备执行环境。
- 父进程负责监控和结果解释。
- 结果映射需要固定顺序，不能随意写。

## 和本仓库 sjudge/ 的对应关系

| demo 概念 | 工程文件 |
| --- | --- |
| 配置结构 | `sjudge/sjudge_call.h` |
| 总控入口 | `sjudge/sjudge_call.cpp` |
| 配置校验 | `sjudge/ConfigValidator.cpp` |
| 子进程准备 | `sjudge/ChildSetup.cpp` |
| 父进程监控 | `sjudge/ParentMonitor.cpp` |
| 结果映射 | `sjudge/ResultMapper.cpp` |
| runner 接入 | `src/runner/RunnerExecutionSupport.cpp` |

## 继续练习

完成这些 demo 后，可以继续做这些练习：

1. 给 `06_mini_judger.cpp` 增加输出大小限制。
2. 把 stderr 重定向到单独文件。
3. 增加 CPU 时间限制判断。
4. 增加 `EXECVE_FAILED`、`DUP2_FAILED` 这类基础设施错误码。
5. 用 `strace` 观察 demo 运行时使用了哪些 syscall。
6. 参考 `seccomp.md` 给 demo 加一个最小 seccomp 白名单。

