# sjudger 安全边界

这篇文档回答一个关键问题：

> 当前 sjudger 到底算不算沙箱？

简短答案：它是一个评测执行核心，但不是完整生产级沙箱。

它已经能控制进程执行、输入输出、资源限制和基础结果映射；但它还没有完成权限隔离、文件系统隔离、系统调用过滤、cgroup 等完整安全边界。

## 当前已经具备的能力

### 进程级执行隔离

通过 `fork()` 创建子进程运行用户程序。

父进程保留在 judger 内部，负责监控和回收子进程。

### 输入输出重定向

通过 `open()` + `dup2()` 把测试输入接到 stdin，把用户输出写入指定文件。

这让用户程序不需要知道测试数据的真实路径。

### 资源限制

当前通过 `setrlimit()` 应用：

- `RLIMIT_STACK`
- `RLIMIT_AS`
- `RLIMIT_FSIZE`
- `RLIMIT_CPU`

真实时间限制由父进程主动监控。

### 结果映射

通过 `ParentMonitor` 和 `ResultMapper`，把底层进程状态映射成：

- SUCCESS
- TLE
- MLE
- RE
- SYSTEM_ERROR

## 当前没有完成的能力

### 没有真实 seccomp

当前 `SeccompPolicyStub.cpp` 是空实现。

这意味着用户程序仍然可以发起系统调用，是否成功取决于当前用户权限和系统环境。

### 没有 uid/gid 降权

用户程序和 judge server 仍以同一系统用户运行。

如果这个用户能访问某些文件，用户程序理论上也可能访问这些文件，除非被文件权限或后续 seccomp 阻止。

### 没有文件系统隔离

当前没有 chroot、mount namespace、只读根文件系统等能力。

`cwd` 只是切换工作目录，不是隔离文件系统。

### 没有 cgroup

`setrlimit()` 对单进程有帮助，但对复杂进程树、总内存、总 CPU、pids 数量等控制不如 cgroup。

### 没有网络隔离

当前没有 network namespace，也没有 seccomp 禁止 socket。

### 没有完整进程树清理

当前监控的是直接 fork 出来的子进程。

如果用户程序再创建子进程，完整清理和限制需要进程组、cgroup 或额外追踪机制。

## 各种隔离技术分别解决什么

| 技术 | 主要解决的问题 |
| --- | --- |
| `setrlimit` | 单进程资源限制 |
| 父进程 real-time monitor | 真实时间限制 |
| seccomp | 系统调用过滤 |
| uid/gid 降权 | 降低文件和系统权限 |
| chroot | 改变进程看到的根目录 |
| mount namespace | 文件系统视图隔离 |
| network namespace | 网络隔离 |
| cgroup | 进程组级资源限制 |
| pid namespace | 进程视图隔离 |

它们是互补关系，不是互相替代。

## 从当前 sjudger 走向生产沙箱的路线

建议按下面顺序增强：

1. 完成真实 seccomp 接入。
2. 使用独立低权限用户运行评测进程。
3. 给子进程设置独立进程组，超时时杀整个进程组。
4. 接入 cgroup v2，限制内存、CPU、pids。
5. 使用 mount namespace 或 chroot 限制文件系统视图。
6. 使用 network namespace 或 seccomp 禁止网络。
7. 固定语言运行时和依赖路径。
8. 增加更细的审计日志和异常诊断。

## 为什么不一开始就做全量沙箱

完整沙箱会显著增加复杂度：

- 需要 root 或 user namespace 配置。
- 需要处理不同 Linux 发行版差异。
- 需要处理编译器、解释器、动态库、标准库路径。
- 测试成本高。

当前仓库优先做的是清晰、可测、能替代外部 `/usr/bin/sjudge` 的内置执行核心。

这是一条合理演进路径：先把执行链路稳定下来，再逐步加安全边界。

## 本地测试时应该怎么理解风险

本地开发时，如果运行的是自己写的测试程序，当前 sjudger 足够验证：

- 执行流程。
- TLE/MLE/RE 映射。
- runner 集成。
- 输出比较。

但不要用它直接运行不可信代码并假设系统安全。

特别是不要在高权限用户下运行 judge server 去执行不可信提交。

