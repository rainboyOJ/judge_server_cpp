# 从零理解 sjudger：一个评测核心是怎样工作的

## 这篇文档要解决什么问题

在线评测系统的核心问题可以简化成一句话：

> 给定一个用户程序、输入文件和资源限制，安全地运行它，并判断它是 AC、TLE、MLE、RE 还是系统错误。

本仓库的 `sjudge/` 就是在做这件事。它不是完整的容器沙箱，也不是生产级隔离系统，但它已经包含了一个 judger 的核心骨架：

1. 父进程负责创建和监控子进程。
2. 子进程负责切换目录、重定向输入输出、设置资源限制，然后执行用户程序。
3. 父进程收集退出状态和资源用量。
4. 结果映射模块把底层状态转换成判题结果。

看完这篇文档，你应该能理解：

1. 为什么 judger 通常要拆成父进程和子进程。
2. `fork()`、`execve()`、`wait4()`、`setrlimit()` 各自解决什么问题。
3. 本仓库 `sjudge/` 每个文件负责哪一层。
4. 如何自己写一个最小可用的 judger。
5. 当前实现还有哪些安全边界没有覆盖。

## 先建立整体模型

一个最小 judger 的运行流程如下：

```text
runner
  |
  | 组装 judge_config
  v
run_sjudger(config)
  |
  | 校验配置
  v
fork()
  |
  +-- child process
  |     |
  |     | chdir / dup2 / setrlimit
  |     v
  |   execve(user program)
  |
  +-- parent process
        |
        | wait4 + real-time monitor
        v
      map result
```

这里最重要的思想是：父进程和子进程职责不同。

子进程会变成用户程序，所以它只负责“把执行环境准备好”。一旦调用 `execve()` 成功，当前进程镜像就被用户程序替换了，后面的 C++ 代码不会继续执行。

父进程不会执行用户代码。它保留在 judger 里，负责观察子进程是否退出、是否超时、用了多少资源。

## 为什么需要 fork 和 execve

### fork 做什么

`fork()` 会复制当前进程，得到两个几乎一样的进程：

- 父进程：`fork()` 返回子进程 pid。
- 子进程：`fork()` 返回 `0`。

所以代码通常长这样：

```cpp
const pid_t pid = fork();
if (pid == 0) {
    // 子进程
}
if (pid < 0) {
    // fork 失败
}
// 父进程
```

judger 使用 `fork()` 的原因是：我们需要一个子进程去运行用户程序，同时父进程继续活着，用来监控它。

### execve 做什么

`execve()` 会把当前进程替换成另一个程序：

```cpp
execve(config.exe_path.c_str(), argv.data(), environ);
```

如果 `execve()` 成功，它不会返回。子进程从这一刻开始就是用户程序。

如果 `execve()` 返回了，说明失败了，比如：

- 可执行文件不存在。
- 没有执行权限。
- 文件格式不对。

本仓库里，子进程会在失败时 `_exit(EXECVE_FAILED)`，让父进程知道这是基础设施错误，不是普通用户程序 RE。

## 为什么输入输出要重定向

用户程序通常从标准输入读数据，向标准输出写答案。题目的输入和用户输出都是文件，所以 judger 要把文件接到标准 fd 上：

- `STDIN_FILENO`：标准输入，fd 0。
- `STDOUT_FILENO`：标准输出，fd 1。
- `STDERR_FILENO`：标准错误，fd 2。

核心操作是：

```cpp
const int fd = open(path.c_str(), flags, 0644);
dup2(fd, STDIN_FILENO);
close(fd);
```

`dup2(fd, STDIN_FILENO)` 的意思是：让标准输入指向这个文件。

本仓库对应代码在 `sjudge/ChildSetup.cpp`：

```cpp
redirect_fd(config.input_path, O_RDONLY, STDIN_FILENO);
redirect_fd(config.output_path, O_WRONLY | O_CREAT | O_TRUNC, STDOUT_FILENO);
redirect_fd(config.error_path, O_WRONLY | O_CREAT | O_TRUNC, STDERR_FILENO);
```

这样用户程序不需要知道测试数据文件在哪里。它只要正常读 `stdin`、写 `stdout`。

## 为什么资源限制放在子进程

资源限制必须应用到用户程序身上，而不是应用到父进程身上。

如果父进程也被限制住，judger 自己可能无法正常监控、杀进程、收集状态。

所以本仓库在子进程里调用 `setrlimit()`：

```cpp
apply_limit(RLIMIT_STACK, config.max_stack_bytes);
apply_limit(RLIMIT_AS, config.max_memory_bytes);
apply_limit(RLIMIT_FSIZE, config.max_output_bytes);
apply_limit(RLIMIT_CPU, cpu_seconds);
```

这些限制会被 `execve()` 后的新程序继承。

### RLIMIT_AS：内存限制

`RLIMIT_AS` 限制进程地址空间大小。它不是简单的“物理内存已使用量”，更接近“这个进程最多能申请多大的虚拟地址空间”。

这会影响：

- `malloc/new`
- Python 解释器启动
- 动态库加载
- 栈和 mmap

所以解释型语言在很低的内存限制下，可能还没运行到用户代码就启动失败。本仓库 `ResultMapper` 对这种情况做了兜底映射。

### RLIMIT_CPU：CPU 时间限制

`RLIMIT_CPU` 的单位是秒，而题目限制通常是毫秒。所以当前实现会把毫秒向上取整到秒：

```cpp
const uint64_t cpu_seconds = (config.max_cpu_time_ms + 999) / 1000;
```

这意味着 `500ms` 的 CPU 限制在内核层会变成 `1s`。毫秒级 CPU 判定还要依赖父进程采集到的 `rusage` 再做结果映射。

### 真实时间限制为什么不能只靠 setrlimit

CPU 时间和真实时间不是一回事。

例子：

```cpp
sleep(100);
```

这个程序真实时间跑了 100 秒，但 CPU 时间可能很少。只靠 `RLIMIT_CPU` 不会及时杀掉它。

所以真实时间限制必须由父进程自己统计：

```cpp
const auto start = std::chrono::steady_clock::now();
```

然后不断检查子进程是否结束。如果超过 `max_real_time_ms`，父进程发送：

```cpp
kill(pid, SIGKILL);
```

本仓库对应代码在 `sjudge/ParentMonitor.cpp`。

## wait4 解决什么问题

父进程需要知道子进程怎么结束的：

- 正常退出了吗？
- exit code 是多少？
- 是不是被 signal 杀死？
- 用了多少 CPU 时间？
- 内存峰值是多少？

`wait4()` 可以同时拿到退出状态和 `rusage`：

```cpp
rusage usage{};
const pid_t wait_rc = wait4(pid, &status, WNOHANG, &usage);
```

`status` 用宏解析：

```cpp
WIFEXITED(status)
WEXITSTATUS(status)
WIFSIGNALED(status)
WTERMSIG(status)
```

`usage` 里常用：

```cpp
usage.ru_utime   // 用户态 CPU 时间
usage.ru_maxrss  // 最大 RSS，Linux 上通常是 KB
```

本仓库把 `ru_maxrss` 转成字节：

```cpp
result.memory_bytes = static_cast<long long>(usage.ru_maxrss) * 1024;
```

## 为什么要单独做 ResultMapper

进程退出状态很底层，不适合直接暴露给 runner。

例如：

- `SIGKILL` 可能表示真实时间超时，也可能表示系统手动杀死。
- 非零退出码通常是 RE。
- 内存限制下的异常信号可能更应该报 MLE。
- 子进程准备阶段失败应该是 SYSTEM_ERROR，而不是用户代码 RE。

所以 `ResultMapper` 把底层状态转换成稳定的判题语义。

当前顺序是：

1. 真实时间超时：`REAL_TIME_LIMIT_EXCEEDED`
2. 内存峰值超过限制：`MEMORY_LIMIT_EXCEEDED`
3. 内存限制下常见内存失败信号：`MEMORY_LIMIT_EXCEEDED`
4. 内存限制下 `exit_code == 127`：`MEMORY_LIMIT_EXCEEDED`
5. CPU 时间超过限制：`CPU_TIME_LIMIT_EXCEEDED`
6. 非零退出码或异常信号：`RUNTIME_ERROR`
7. 其他：`SUCCESS`

顺序很重要。比如一个程序因为超时被 `SIGKILL`，如果先判断 signal，就可能误报 RE；先判断 `timed_out`，才能稳定报 TLE。

## 本仓库代码如何对应这些概念

### JudgeTypes.h

这是数据模型头文件。

关键类型：

```cpp
struct judge_config;
struct judge_result;
```

结果码和错误码也在这里定义。阅读代码时建议先看这个文件，因为后面所有模块都围绕这些类型协作。

### SJudger.h

这是内置执行核心的对外入口头文件。

关键接口：

```cpp
judge_result run_sjudger(const judge_config &config);
```

runner 层只需要知道这几个东西，不需要知道底层用了 `fork()` 还是 `wait4()`。

### sjudge_call.h

这是兼容头文件。它同时 include `SJudger.h` 和 `ExternalSjudgeCompat.h`，让旧代码继续通过 `sjudge_call.h` 拿到入口声明。

### sjudge_call.cpp

这是总控入口。

它负责把几个模块串起来：

1. `validate_judge_config()`
2. `fork()`
3. 子进程调用 `run_child_process_or_exit()`
4. 父进程调用 `monitor_child_process()`
5. 基础设施错误识别
6. `map_monitor_result_to_judge_code()`

它不应该塞太多细节。细节越多，越难测试和维护。

### ExternalSjudgeCompat

这个模块保留旧的 `call_sjudge()` 外部命令调用逻辑。

当前 runner 已经走内置 `run_sjudger()`，所以学习内置 sjudger 时可以先跳过它。

### ConfigValidator

负责配置合法性。

当前规则很少，主要是 `exe_path` 不能为空。后续如果要限制路径、参数、工作目录，也应该优先放到这里。

### ChildSetup

只在子进程中运行。

职责：

1. `chdir(config.cwd)`
2. `dup2()` 重定向标准输入输出
3. `setrlimit()` 应用资源限制
4. 组装 `argv`
5. `execve()`

这个模块里不能依赖复杂的父进程状态。因为一旦进入子进程，最好尽快完成准备并 `execve()`。

### ParentMonitor

只在父进程中运行。

职责：

1. 轮询 `wait4(WNOHANG)`
2. 计算真实时间
3. 超时后 `kill(pid, SIGKILL)`
4. 采集 `exit_code/signal/cpu_time/memory`

它不直接决定 AC/TLE/MLE/RE，只返回 `monitor_result`。

### ResultMapper

负责判题结果语义。

如果后续发现某类语言有特殊退出模式，优先在这里调整映射，而不是把逻辑散落到 runner 或 ParentMonitor。

### RunnerExecutionSupport

这个文件不属于 `sjudge/`，但它是 runner 接入 sjudger 的桥。

它负责：

1. 检查可执行文件、输入文件、标准输出文件是否存在。
2. 把 `RunnerCaseInput` 转成 `judge_config`。
3. 调用 `run_sjudger()`。
4. 把 sjudger result code 转成 `SubmissionVerdict`。
5. 如果执行成功，再比较输出。

## 从零写一个最小 judger

下面是一段伪代码，用来说明最小 judger 需要哪些步骤。

```cpp
struct Config {
    std::string exe;
    std::string input;
    std::string output;
    int real_time_ms;
    uint64_t memory_bytes;
};

struct Result {
    int verdict;
    int exit_code;
    int signal;
    int real_time_ms;
    long long memory_bytes;
};

Result judge(const Config &config) {
    pid_t pid = fork();

    if (pid == 0) {
        int in = open(config.input.c_str(), O_RDONLY);
        int out = open(config.output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        dup2(in, STDIN_FILENO);
        dup2(out, STDOUT_FILENO);
        close(in);
        close(out);

        if (config.memory_bytes > 0) {
            rlimit limit{};
            limit.rlim_cur = config.memory_bytes;
            limit.rlim_max = config.memory_bytes;
            setrlimit(RLIMIT_AS, &limit);
        }

        char *argv[] = {const_cast<char *>(config.exe.c_str()), nullptr};
        execve(config.exe.c_str(), argv, environ);
        _exit(127);
    }

    if (pid < 0) {
        return Result{/* SYSTEM_ERROR */};
    }

    auto start = steady_clock::now();
    while (true) {
        int status = 0;
        rusage usage{};
        pid_t rc = wait4(pid, &status, WNOHANG, &usage);

        if (rc == pid) {
            return map_status(status, usage);
        }

        if (elapsed_ms(start) > config.real_time_ms) {
            kill(pid, SIGKILL);
            wait4(pid, &status, 0, &usage);
            return Result{/* TLE */};
        }

        usleep(1000);
    }
}
```

这段代码不完整，但它包含了核心骨架：

1. `fork()` 创建子进程。
2. 子进程准备环境。
3. 子进程 `execve()` 用户程序。
4. 父进程 `wait4()` 监控。
5. 父进程超时杀进程。
6. 最后映射结果。

本仓库的 sjudger 就是把这段骨架拆成了更清晰的模块。

## 自己实现 judger 时最容易踩的坑

### 把真实时间和 CPU 时间混为一谈

死循环主要消耗 CPU，`sleep()` 主要消耗真实时间。两者都需要限制。

### 在父进程设置资源限制

这样会限制 judger 自己。资源限制应该在子进程里、`execve()` 前设置。

### 忘记处理 execve 失败

`execve()` 成功不会返回。返回就说明失败，必须 `_exit()` 一个明确错误码。

### 子进程里调用复杂逻辑

`fork()` 后的子进程应该尽快 `execve()`。复杂日志、锁、线程状态都可能带来意外问题。

### 直接把 signal 当成 RE

signal 需要结合上下文判断。超时产生的 `SIGKILL` 应该是 TLE，不是 RE。

### 认为内存峰值一定会超过限制

内存限制可能导致程序在申请内存时直接失败，也可能让解释器启动失败。此时 `ru_maxrss` 不一定超过限制，需要结合 signal 或 exit code 映射。

### 忽视输出大小限制

没有 `RLIMIT_FSIZE` 或类似限制时，恶意程序可以无限写输出文件，占满磁盘。

## 安全边界

当前 sjudger 是“执行控制核心”，不是完整安全沙箱。

它已经做了：

- 进程级执行隔离。
- 标准输入输出重定向。
- CPU、真实时间、内存、栈、输出大小限制。
- 基础退出状态映射。

它仍然没有做：

- chroot 或 mount namespace。
- network namespace。
- 文件系统只读隔离。
- 允许派生进程时的多进程树完整清理。

它已经提供可选能力：

- seccomp 禁网络/禁派生进程。
- uid/gid 降权。
- cgroup v2 加入和 memory/pids/cpu 限制。

所以它适合当前仓库的本地测试和基础评测链路，但不能直接等价为生产级沙箱。

## 学习路线

如果你想真正自己写一个 judger，建议按这个顺序练习：

1. 写一个程序，只用 `fork()` + `execve()` 执行 `/bin/echo`。
2. 加上 `dup2()`，让 `/bin/cat` 从文件读输入，把输出写到文件。
3. 加上 `wait4()`，打印 exit code、signal、CPU 时间、内存峰值。
4. 加上真实时间监控，运行死循环程序并杀掉它。
5. 加上 `setrlimit(RLIMIT_AS)`，运行内存分配程序并观察结果。
6. 加上 `setrlimit(RLIMIT_FSIZE)`，运行无限输出程序并观察结果。
7. 写一个 `map_result()`，把底层状态映射成 AC/TLE/MLE/RE。
8. 最后再考虑 seccomp、降权、namespace、cgroup。

这个顺序和本仓库 `sjudge/` 的模块拆分基本一致。
