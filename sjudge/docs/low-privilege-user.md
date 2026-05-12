# sjudge 低权限用户配置指南

这篇文档记录 simple OJ 推荐的 `sjudge` 安全部署思路：

```text
不使用 cgroup
手动创建低权限用户
sjudge 子进程 setgid/setuid 到该用户
同时使用 setrlimit + seccomp 禁网络/禁派生进程
```

这个方案适合个人 OJ、课程 OJ、内网小型 OJ。它比“用户程序和 judge_server 同用户运行”安全很多，但仍不是完整生产级沙箱。

## 目标模型

推荐模型：

```text
judge_server
  |
  | fork()
  v
sjudge child process
  |
  | chdir
  | redirect stdin/stdout/stderr
  | setrlimit
  | setgid(ojrun_gid)
  | setuid(ojrun_uid)
  | seccomp 禁网络/禁 fork
  v
user program
```

父进程继续保留 judge_server 的运行权限，用来：

- `wait4()` 监控子进程。
- 超时时 `kill()` 子进程。
- 收集 exit code、signal、CPU、memory。

子进程在 `execve()` 用户程序前降权，最终用户代码以低权限用户运行。

## 创建低权限用户

建议创建一个专门运行用户程序的系统用户，例如 `ojrun`：

```bash
sudo useradd -r -M -s /usr/sbin/nologin ojrun
id ojrun
```

输出类似：

```text
uid=995(ojrun) gid=995(ojrun) groups=995(ojrun)
```

记录这里的 uid/gid，后面填入 `config/config.json` 的 `sjudge` 小节。

## judge_server 配置

当前 `judge_server` 会从 `config/config.json` 读取 `sjudge` 小节，并在每次执行测试点时写入 `judge_config`：

```json
{
  "sjudge": {
    "run_uid": 995,
    "run_gid": 995,
    "clear_supplementary_groups": true,
    "enable_seccomp": true,
    "seccomp_deny_network": true,
    "seccomp_deny_process_spawn": true,
    "cgroup_path": "",
    "cgroup_memory_max_bytes": 0,
    "cgroup_pids_max": 0,
    "cgroup_cpu_max_quota_us": 0,
    "cgroup_cpu_max_period_us": 0
  }
}
```

simple OJ v1 可以先让 `cgroup_path` 为空，只使用 `setrlimit + uid/gid + seccomp`。

## judge_config 对应关系

示例：

```cpp
judge_config config{};
config.exe_path = executable_path;
config.args = {executable_path};
config.input_path = input_path;
config.output_path = output_path;
config.error_path = error_path;
config.cwd = work_dir;

config.max_real_time_ms = 1000;
config.max_cpu_time_ms = 1000;
config.max_memory_bytes = 256ULL * 1024 * 1024;
config.max_stack_bytes = 64ULL * 1024 * 1024;
config.max_output_bytes = 64ULL * 1024 * 1024;

config.run_uid = 995;
config.run_gid = 995;
config.clear_supplementary_groups = true;

config.enable_seccomp = true;
config.seccomp_deny_network = true;
config.seccomp_deny_process_spawn = true;

// simple OJ v1 可以不使用 cgroup。
config.cgroup_path.clear();
```

字段含义：

| 字段 | 含义 |
| --- | --- |
| `run_uid` | 用户程序最终运行的 uid |
| `run_gid` | 用户程序最终运行的 gid |
| `clear_supplementary_groups` | setgid/setuid 前清空附加组 |
| `enable_seccomp` | 是否加载 seccomp 策略 |
| `seccomp_deny_network` | 禁止 `socket/connect/...` |
| `seccomp_deny_process_spawn` | 禁止 `fork/clone/vfork` |
| `cgroup_path` | 为空表示不使用 cgroup |

## 权限要求

普通用户不能随便 `setuid()` 成另一个用户。

因此常见部署方式是：

```text
judge_server 以 root 启动
fork 后的子进程 setgid/setuid 到 ojrun
用户程序以 ojrun 身份 execve
```

注意：

- 不要让用户程序以 root 身份运行。
- 如果 `setgid()` 或 `setuid()` 失败，`sjudge` 会返回 `SYSTEM_ERROR`，错误码为 `SETUID_FAILED`。
- 如果 judge_server 不是 root，通常只能 setuid 到自己，不能切到 `ojrun`。

## 工作目录权限

降权后，用户程序只拥有 `ojrun` 的权限。

因此工作目录、输入文件、输出文件、可执行文件必须让 `ojrun` 可访问。

建议每个 case 使用独立工作目录：

```text
/tmp/oj_compile_<submission_id>/
  solution
  case.in
  case.out
  case.err
```

推荐权限：

```bash
sudo chown -R ojrun:ojrun /tmp/oj_compile_<submission_id>
sudo chmod 700 /tmp/oj_compile_<submission_id>
sudo chmod 700 /tmp/oj_compile_<submission_id>/solution
```

如果使用脚本 wrapper，例如 Python 的 `run_python.sh`，也需要可执行权限：

```bash
chmod 700 run_python.sh
```

如果权限不对，常见错误是：

- `DUP2_FAILED`
- `EXECVE_FAILED`
- 用户程序运行时无法读写文件

## 题目数据访问建议

不要直接把整个 `testData` 目录暴露给低权限用户。

更推荐：

1. judge_server 读取题目输入。
2. 把当前 case 的 input 复制到工作目录。
3. 只给 `ojrun` 访问本次 case 所需文件。
4. 用户输出也写在工作目录。

这样即使用户程序尝试遍历目录，也只能看到本次 case 的文件。

## 为什么可以不使用 cgroup

simple OJ v1 可以不使用 cgroup，依赖：

- `RLIMIT_AS`
- `RLIMIT_CPU`
- `RLIMIT_STACK`
- `RLIMIT_FSIZE`
- 父进程 real-time timeout
- uid/gid 降权
- seccomp 禁网络
- seccomp 禁派生进程

如果你显式禁止派生进程：

```cpp
config.seccomp_deny_process_spawn = true;
```

那么不用 cgroup 的风险会小很多，因为用户程序不能再 fork 出复杂进程树。

不用 cgroup 会少这些能力：

- 进程组级总内存控制。
- `pids.max`。
- `cgroup.kill` 清理。
- 更稳定的多进程资源统计。

但对于 simple OJ，如果禁止 fork，这个取舍可以接受。

## 推荐运行顺序

`sjudge` 子进程当前推荐顺序是：

```text
chdir
redirect stdin/stdout/stderr
setrlimit
可选 cgroup
setgroups(0)
setgid
setuid
seccomp
execve
```

为什么这个顺序合理：

- IO 重定向和 cgroup 写入可能需要较高权限，所以放在降权前。
- 降权必须放在 `execve` 前，保证用户程序不是高权限运行。
- seccomp 放在最后，避免提前禁止 `setuid`、`setgid`、`open` 等准备步骤。

## 部署检查清单

上线前检查：

- 已创建 `ojrun`。
- 已记录正确 uid/gid。
- judge_server 有权限执行 setuid/setgid。
- 每个 case 工作目录归属 `ojrun`。
- 用户程序可执行文件对 `ojrun` 可执行。
- 输入文件对 `ojrun` 可读。
- 输出/错误文件路径对 `ojrun` 可写。
- `enable_seccomp = true`。
- `seccomp_deny_network = true`。
- `seccomp_deny_process_spawn = true`。
- 不在 judge 机器上放敏感文件。

## 当前仍不是完整沙箱

这个方案仍然没有：

- chroot。
- mount namespace。
- network namespace。
- 只读根文件系统。
- cgroup 总资源控制。

所以更准确的定位是：

```text
simple OJ v1 的低权限执行模型
```

而不是：

```text
公网生产级强沙箱
```
