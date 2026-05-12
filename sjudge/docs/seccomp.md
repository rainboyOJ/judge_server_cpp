# seccomp 教学：给 judger 加一道系统调用边界

## 这篇文档要解决什么问题

`setrlimit()` 能限制 CPU、内存、输出文件大小，但它不能限制用户程序“调用哪些内核能力”。

例如，一个恶意或不受信任的程序可能尝试：

- 打开不该访问的文件。
- 创建网络连接。
- 创建大量子进程。
- 修改文件权限。
- 调用危险或不必要的系统调用。

`seccomp` 的作用就是限制进程可以使用的系统调用。它不是资源限制，而是系统调用过滤。

这篇文档会讲：

1. 什么是系统调用。
2. seccomp 解决什么问题。
3. seccomp 和 `setrlimit()` 的区别。
4. libseccomp 的基本使用方式。
5. judger 中应该把 seccomp 放在哪里。
6. 如何为 C++/Python 程序设计一个最小策略。
7. 本项目 `sjudge/` 后续如何接入真实 seccomp。

当前项目状态：`sjudge/SeccompPolicyStub.cpp` 仍是空实现，真实 seccomp 策略还没有完成。所以本文是教学和实现指南，不表示当前代码已经具备系统调用过滤能力。

## 先理解系统调用

普通程序不能直接操作硬件、文件系统、进程表和网络。它需要通过内核提供的接口完成这些动作，这些接口就是系统调用。

常见系统调用：

| 系统调用 | 作用 |
| --- | --- |
| `read` | 从文件描述符读取数据 |
| `write` | 向文件描述符写数据 |
| `openat` | 打开文件 |
| `close` | 关闭文件描述符 |
| `execve` | 执行新程序 |
| `mmap` | 映射内存 |
| `brk` | 调整堆空间 |
| `clone` | 创建线程或进程 |
| `socket` | 创建网络 socket |
| `connect` | 发起网络连接 |
| `kill` | 发送信号 |

一个用户程序想读 stdin，本质上会调用 `read(0, ...)`。

一个用户程序想写 stdout，本质上会调用 `write(1, ...)`。

一个 Python 解释器启动时，会调用更多系统调用，例如 `openat`、`mmap`、`fstat`、`readlink`、`getrandom` 等。

## seccomp 的核心思想

seccomp 的核心思想是：

> 给进程安装一个过滤器。之后进程每次发起系统调用，内核先检查过滤器。允许就继续执行，不允许就拒绝、杀死或返回错误。

常见策略有两种：

1. 黑名单
   - 默认允许所有系统调用。
   - 单独禁止危险调用。
2. 白名单
   - 默认禁止所有系统调用。
   - 只允许明确列出的调用。

judger 更适合白名单。

原因是用户代码不可信，默认允许太多能力风险很大。白名单虽然调试成本更高，但边界更清楚。

## seccomp 和 setrlimit 的区别

| 能力 | setrlimit | seccomp |
| --- | --- | --- |
| 限制 CPU 时间 | 可以 | 不适合 |
| 限制内存 | 可以 | 不适合 |
| 限制输出文件大小 | 可以 | 不适合 |
| 禁止网络 | 不可以 | 可以 |
| 禁止 fork/clone | 不可以 | 可以 |
| 禁止打开文件 | 不可以 | 可以 |
| 限制某个 syscall 参数 | 不可以 | 可以 |

它们不是替代关系，而是互补关系。

一个 judger 的常见组合是：

1. `setrlimit()` 限制资源。
2. seccomp 限制系统调用能力。
3. uid/gid、namespace、cgroup 等提供更强隔离。

## libseccomp 是什么

Linux 原生 seccomp 过滤器使用 BPF 表达，直接写比较繁琐。`libseccomp` 是一个更易用的 C API 封装。

典型使用流程：

```cpp
#include <seccomp.h>

scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
seccomp_load(ctx);
seccomp_release(ctx);
```

含义：

1. `seccomp_init(SCMP_ACT_KILL)`
   - 创建过滤器。
   - 默认动作是杀死进程。
2. `seccomp_rule_add(..., SCMP_ACT_ALLOW, SCMP_SYS(read), 0)`
   - 允许 `read` 系统调用。
3. `seccomp_load(ctx)`
   - 把过滤器加载进内核。
   - 加载成功后，当前进程之后的系统调用都会受限制。
4. `seccomp_release(ctx)`
   - 释放用户态对象。
   - 不会卸载已经加载到内核的过滤器。

## seccomp 应该放在 judger 的哪里

seccomp 必须在子进程里启用，并且通常放在 `execve()` 之前。

本项目子进程准备流程应该是：

```text
child process
  |
  | chdir
  | dup2 stdin/stdout/stderr
  | setrlimit
  | apply_seccomp_if_enabled
  v
execve(user program)
```

为什么不是父进程？

因为父进程还要调用 `wait4()`、`kill()`、记录日志、管理结果。如果父进程也被 seccomp 限制住，它可能无法完成监控职责。

为什么在 `execve()` 前？

因为 seccomp 过滤器会被 `execve()` 后的新程序继承。这样用户程序一启动就已经处在限制之下。

## 最小 C++ 程序需要哪些 syscall

一个极简静态链接 C/C++ 程序可能只需要很少系统调用：

- `read`
- `write`
- `exit`
- `exit_group`
- `brk`
- `mmap`
- `munmap`
- `fstat`
- `close`

但普通动态链接程序启动时还需要动态加载器参与，所以通常还会用到：

- `openat`
- `access`
- `newfstatat`
- `readlink`
- `mprotect`
- `arch_prctl`
- `set_tid_address`
- `set_robust_list`
- `rseq`
- `prlimit64`
- `getrandom`

不同 Linux 发行版、glibc 版本、编译选项都会影响 syscall 列表。

所以 seccomp 白名单不应该靠猜。应该通过工具观察。

## 如何观察程序用了哪些 syscall

最直接的工具是 `strace`：

```bash
strace -f -o trace.log ./solution < input.txt > output.txt
```

查看系统调用名称：

```bash
awk '{print $2}' trace.log | sed 's/(.*//' | sort -u
```

如果是 Python：

```bash
strace -f -o trace.log python3 solution.py < input.txt > output.txt
```

Python 会比 C++ 需要更多 syscall，因为解释器启动、导入模块、读取脚本都要访问文件系统和运行时环境。

## 为什么 Python 的 seccomp 更复杂

C++ 编译产物通常比较直接：程序启动后运行自己的逻辑。

Python runner 当前是：

```text
run_python.sh -> python3 solution.py
```

这意味着 seccomp 策略不仅要允许用户脚本执行，还要允许：

- `/bin/sh` 启动 wrapper。
- `python3` 解释器启动。
- 动态库加载。
- Python 读取标准库或运行时文件。
- Python 打开用户脚本文件。

如果策略太严格，Python 可能在用户代码运行前就被杀死。

这也是为什么真实 judger 常常会：

1. 按语言选择不同 seccomp 策略。
2. 尽量减少 wrapper 层。
3. 固定运行时路径。
4. 对解释型语言做更多环境准备。

当前项目为了保持首版简单，设计上还没有暴露运行时 rule name。后续可以先做一个默认策略，再决定是否按语言拆分。

## 常见 seccomp 动作

libseccomp 常见动作：

| 动作 | 含义 |
| --- | --- |
| `SCMP_ACT_ALLOW` | 允许 syscall |
| `SCMP_ACT_KILL` | 违规时杀死进程 |
| `SCMP_ACT_ERRNO(errno)` | 违规时让 syscall 返回指定 errno |
| `SCMP_ACT_TRAP` | 发送 `SIGSYS` |
| `SCMP_ACT_LOG` | 记录日志但允许执行，依赖内核支持 |

judger 首版通常可以用：

```cpp
seccomp_init(SCMP_ACT_KILL);
```

优点是边界明确。

缺点是调试时只看到进程被杀，可能不清楚缺了哪个 syscall。调试阶段可以临时用 `SCMP_ACT_ERRNO(EPERM)` 或 `strace` 辅助。

## 按参数限制 syscall

seccomp 不只能按 syscall 名过滤，也可以按参数过滤。

例如，只允许写 stdout/stderr：

```cpp
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1,
                 SCMP_A0(SCMP_CMP_EQ, STDOUT_FILENO));
seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 1,
                 SCMP_A0(SCMP_CMP_EQ, STDERR_FILENO));
```

`SCMP_A0` 表示第 0 个参数。

`write(fd, buf, count)` 的第 0 个参数就是 `fd`。

这种限制更细，但也更容易误伤。例如动态链接器或运行时可能要写其他 fd。首版可以先只按 syscall 名白名单，后续再按参数收紧。

## 在本项目中接入 seccomp 的建议实现

当前接口已经预留：

```cpp
bool apply_seccomp_if_enabled(const judge_config &config, int &error_code);
```

建议新增真实实现文件：

```text
sjudge/SeccompPolicy.cpp
```

基础结构：

```cpp
#include "SeccompPolicy.h"

#include <seccomp.h>

bool apply_seccomp_if_enabled(const judge_config &, int &error_code) {
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
    if (ctx == nullptr) {
        error_code = LOAD_SECCOMP_FAILED;
        return false;
    }

    const int allow_rules[] = {
        SCMP_SYS(read),
        SCMP_SYS(write),
        SCMP_SYS(exit),
        SCMP_SYS(exit_group),
        SCMP_SYS(brk),
        SCMP_SYS(mmap),
        SCMP_SYS(munmap),
        SCMP_SYS(fstat),
        SCMP_SYS(close),
        SCMP_SYS(openat),
        SCMP_SYS(access),
        SCMP_SYS(newfstatat),
        SCMP_SYS(readlink),
        SCMP_SYS(mprotect),
        SCMP_SYS(arch_prctl),
        SCMP_SYS(set_tid_address),
        SCMP_SYS(set_robust_list),
        SCMP_SYS(prlimit64),
        SCMP_SYS(getrandom),
    };

    for (const int syscall_id : allow_rules) {
        if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscall_id, 0) != 0) {
            seccomp_release(ctx);
            error_code = LOAD_SECCOMP_FAILED;
            return false;
        }
    }

    if (seccomp_load(ctx) != 0) {
        seccomp_release(ctx);
        error_code = LOAD_SECCOMP_FAILED;
        return false;
    }

    seccomp_release(ctx);
    error_code = 0;
    return true;
}
```

注意：这只是教学骨架，不是最终策略。真实策略需要结合 `strace` 和测试逐步补齐。

## CMake 应该怎么接

目标行为：

```cmake
option(SJUDGER_ENABLE_SECCOMP "Build sjudger with seccomp support" ON)
```

当打开：

1. 查找 `libseccomp`。
2. 编译 `sjudge/SeccompPolicy.cpp`。
3. 链接 seccomp。
4. 定义 `SJUDGER_ENABLE_SECCOMP=1`。

当关闭：

1. 编译 `sjudge/SeccompPolicyStub.cpp`。
2. 不链接 libseccomp。

示意：

```cmake
if(SJUDGER_ENABLE_SECCOMP)
    find_library(SECCOMP_LIB seccomp REQUIRED)
    list(APPEND SJUDGER_SOURCES sjudge/SeccompPolicy.cpp)
else()
    list(APPEND SJUDGER_SOURCES sjudge/SeccompPolicyStub.cpp)
endif()

if(SJUDGER_ENABLE_SECCOMP)
    target_compile_definitions(bt_runner PUBLIC SJUDGER_ENABLE_SECCOMP=1)
    target_link_libraries(bt_runner PUBLIC ${SECCOMP_LIB})
endif()
```

当前仓库还没有完成这一步。现状是 stub 始终参与编译。

## ChildSetup 应该在哪里调用

真实接入后，`ChildSetup.cpp` 应该在 `execve()` 前调用：

```cpp
int seccomp_error = 0;
if (!apply_seccomp_if_enabled(config, seccomp_error)) {
    _exit(seccomp_error);
}

execve(config.exe_path.c_str(), argv.data(), environ);
```

注意调用顺序：

1. 先完成 `open()` 和 `dup2()`。
2. 先完成 `setrlimit()`。
3. 再加载 seccomp。
4. 最后 `execve()`。

原因是如果 seccomp 策略禁止 `openat` 或某些资源相关 syscall，加载太早会让子进程自己也无法完成准备工作。

## 如何测试 seccomp 是否生效

可以写一个试图做禁止动作的程序。

例如禁止 `socket` 后，测试程序：

```cpp
#include <sys/socket.h>

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    return fd < 0 ? 1 : 0;
}
```

期望：

- 如果策略是 `SCMP_ACT_KILL`，程序应被 signal 终止。
- sjudger 最终应返回非 `SUCCESS`。

也可以测试禁止 fork：

```cpp
#include <unistd.h>

int main() {
    pid_t pid = fork();
    return pid < 0 ? 1 : 0;
}
```

如果 `clone/fork/vfork` 不在白名单中，它应该失败或被杀。

## 调试策略

seccomp 调试通常是迭代式的：

1. 先不用 seccomp 跑通程序。
2. 用 `strace -f` 收集 syscall 列表。
3. 写最小白名单。
4. 开启 seccomp。
5. 如果程序被杀，继续用 `strace` 或临时放宽策略定位缺失 syscall。
6. 为缺失 syscall 加测试或注释说明。

不要一开始就追求极限严格。先让 C++ 和 Python 的基础执行路径稳定，再逐步收紧。

## 一个合理的项目落地顺序

建议按下面步骤接入本仓库：

1. 新增 `sjudge/SeccompPolicy.cpp`，实现最小白名单。
2. 修改 `CMakeLists.txt`，根据 `SJUDGER_ENABLE_SECCOMP` 选择真实实现或 stub。
3. 在 `ChildSetup.cpp` 的 `execve()` 前调用 `apply_seccomp_if_enabled()`。
4. 增加 `test_sjudger` 条件测试，只在 `SJUDGER_ENABLE_SECCOMP` 定义时运行。
5. 先验证 C++ 简单程序。
6. 再验证 Python wrapper。
7. 如果 Python 白名单过大，考虑后续拆分按语言策略。

## 当前项目的安全提示

seccomp 只是沙箱的一部分。

即使接入 seccomp，也不能自动解决：

- 文件系统隔离。
- 用户权限隔离。
- 进程树清理。
- cgroup 限制。
- 网络 namespace。
- root 权限风险。

所以完整生产沙箱通常还需要：

- 非特权用户运行。
- `setuid/setgid` 或 user namespace。
- mount namespace 或 chroot。
- cgroup v2。
- seccomp。
- 严格的工作目录和文件权限管理。

本项目的合理目标是先做一个清晰、可测、可演进的内置执行核心，再逐步增强隔离能力。

