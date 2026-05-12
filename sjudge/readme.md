# sjudge 教学阅读路线

`sjudge/` 是本仓库内置的评测执行核心。它负责运行用户程序、应用资源限制、监控进程状态，并把底层退出状态映射成判题结果。

如果你想系统理解它，建议按下面顺序阅读：

1. [`docs/tutorial.md`](./docs/tutorial.md)
   - 总体原理。
   - 先理解 judger 为什么要拆成父进程监控和子进程执行。
2. [`docs/from-scratch-mini-judger.md`](./docs/from-scratch-mini-judger.md)
   - 动手从零写一个最小 judger。
   - 对应代码在 [`demo_code_for_tutorial/`](./demo_code_for_tutorial/)。
3. [`docs/result-mapping.md`](./docs/result-mapping.md)
   - 专门解释 AC/TLE/MLE/RE/SYSTEM_ERROR 的判定顺序。
4. [`docs/seccomp.md`](./docs/seccomp.md)
   - 学习 seccomp 系统调用过滤。
   - 当前项目仍是 stub，这份文档用于理解后续如何接入真实 seccomp。
5. [`docs/security-boundary.md`](./docs/security-boundary.md)
   - 理解当前 sjudger 的安全边界。
   - 区分 `setrlimit`、seccomp、uid/gid、namespace、cgroup 等能力。
6. [`docs/debugging.md`](./docs/debugging.md)
   - 学习怎么排查 `DUP2_FAILED`、`EXECVE_FAILED`、TLE、MLE、RE 等问题。

## demo 代码

教学 demo 放在：

```text
sjudge/demo_code_for_tutorial/
```

建议先进入目录：

```bash
cd sjudge/demo_code_for_tutorial
make
```

然后按编号运行：

```bash
./01_fork_exec
./02_redirect_io
./03_wait4_usage
./04_realtime_limit
./05_rlimit_memory
./06_mini_judger
```

这些 demo 不是生产代码，而是为了帮助理解 `sjudge/` 的核心原理。真正的工程实现请看 `sjudge/*.cpp`。

## 代码阅读顺序

理解实现时建议按这个顺序读代码：

1. [`JudgeTypes.h`](./JudgeTypes.h)
   - 先看 `judge_config`、`judge_result`、结果码和错误码。
2. [`SJudger.h`](./SJudger.h)
   - 看内置执行核心对外入口 `run_sjudger()`。
3. [`sjudge_call.cpp`](./sjudge_call.cpp)
   - 看总控流程：校验配置、`fork()`、父子进程分工、结果组装。
4. [`ConfigValidator.cpp`](./ConfigValidator.cpp)
   - 看配置合法性检查。
5. [`ChildSetup.cpp`](./ChildSetup.cpp)
   - 看子进程如何 `chdir`、重定向 IO、应用资源限制、加载 seccomp、`execve()`。
6. [`ParentMonitor.cpp`](./ParentMonitor.cpp)
   - 看父进程如何 `wait4()`、统计真实时间、超时杀进程、采集资源。
7. [`ResultMapper.cpp`](./ResultMapper.cpp)
   - 看底层状态如何映射成 SUCCESS/TLE/MLE/RE。
8. [`ExternalSjudgeCompat.cpp`](./ExternalSjudgeCompat.cpp)
   - 这是旧外部 `/usr/bin/sjudge` 兼容层；理解内置 sjudger 时可以最后看。
9. [`../src/runner/RunnerExecutionSupport.cpp`](../src/runner/RunnerExecutionSupport.cpp)
   - 看 runner 如何把 `RunnerCaseInput` 转成 `judge_config` 并调用 `run_sjudger()`。
