# py-judge-runner

Linux 单程序资源执行器：启动已存在的程序，重定向标准流，用 cgroup v2 管理内存，
用 `wait4` 统计 CPU，用 wall-clock 看门狗防止卡死，最后返回 JSON 结果。
不编译提交、不比较答案。`OK` 只表示正常执行。

## 先理解两个限制

**题目阈值用于最终判定，保护上限用于阻止失控。** 两者分开，让稍微超限的程序
可以完成执行，再根据真实统计判断；严重超限则由内核或看门狗终止。

| 资源 | 默认题目阈值 | 默认保护设置 | 最终判定 |
|---|---|---|---|
| CPU | 1000ms | 加 200ms，再向上取整到秒：soft 2秒，hard 3秒 | 原始 CPU 微秒数 > 1000000，或 SIGXCPU → TLE |
| 内存 | 128MiB | 加 16MiB：`memory.max = 144MiB` | `memory.peak > 128MiB` 或 OOM 事件 → MLE |
| wall | 独立保护限制 | 默认 CPU 题目阈值 + 500ms，即 1500ms | 看门狗触发 → TLE |

CPU soft 到期发 SIGXCPU，hard 比 soft 多一秒用于兜底。`RLIMIT_CPU` 只支持整数秒，
所以“200ms 余量”不会产生精确的 1200ms 内核截止时刻。wall 包括 I/O、调度、等待，
与 CPU 是不同的量；可用 `--wall-time` 单独设置。

内存采用 **cgroup 整个提交的内存口径**，包含后代进程、文件缓存和部分内核内存。
共享页按内核的归属记账，不是把各进程 RSS 简单相加。关闭本组 swap，避免换出后
内存计量口径变化。上级 cgroup 的限制仍会生效，应给执行器及各提交留足总资源。

`RLIMIT_AS` 与 `--as-factor` 已删除，避免虚拟地址空间先触顶而使实际内存判定失真。
`rss_kb` 仍保留作诊断，但不用于 MLE 判定。

## 构建与权限

需要 Linux cgroup v2，以及 `memory.peak`、`memory.swap.max`、`memory.oom.group`、
`cgroup.kill` 接口；Python 3.8+，无第三方 Python 依赖。
首次构建 helper 需要 C 编译器和 make，运行时不需要编译器。

```bash
cd py-judge-runner
make
```

调用者必须提供**已委派、可创建子组、且为子组启用 memory controller** 的父目录。
runner 只创建和清理自己命名为 `case-*` 的子组，不修改其他组。
没有权限或接口不完整时返回 `SYSTEM_ERROR`，不会退回 RSS 或无内存限制的运行。

本机支持 systemd 用户委派，可通过示例启动一个专用 scope：

```bash
systemd-run --user --scope -p Delegate=yes -- \
  python3 examples/delegated.py python3 runner.py \
  --input 1.in --output 1.user.out --time 1000 --memory 128 -- ./solution
```

`examples/delegated.py` 先把自身及随后启动的执行器放入 `manager` 叶子组，再为 scope
父目录启用 memory controller。这样执行器与各提交是兄弟组，执行器自己的内存
不会计入提交。该脚本仅用于自身独占的新 scope。

生产部署可由服务管理器预先提供委派目录，然后直接执行：

```bash
python3 runner.py --cgroup-root /sys/fs/cgroup/your-delegated-parent \
  --input 1.in --output 1.user.out \
  --time 1000 --cpu-slack-ms 200 --memory 128 --memory-slack 16 -- ./solution
```

也可设置环境变量 `ROJ_JUDGE_CGROUP_ROOT`。未指定时使用 `/sys/fs/cgroup/roj-judge`；
这个默认路径也必须由部署环境预先准备，runner 不会自行配置系统目录。

## Python 接口

```python
from pathlib import Path
from runner import Limits, run_case

result = run_case(
    ["./solution", "argument"],
    Path("1.in"),
    Path("1.user.out"),
    Limits(
        time_ms=1000,
        cpu_slack_ms=200,
        memory_kb=128 * 1024,
        memory_slack_kb=16 * 1024,
    ),
    cwd=Path("/work"),
    cgroup_root=Path("/sys/fs/cgroup/your-delegated-parent"),
)
print(result.to_dict())
```

文件路径相对于调用者当前目录，命令中的 `./solution` 相对于 `cwd`，命令名支持 PATH。
参数原样传递；不把命令拼成 shell 字符串。stdout 写入 `--output`，stderr 默认是
`OUTPUT.err`，可用 `--stderr` 覆盖。三个标准流不能指向同一文件。

root 默认设置重定向及限制后降权至 nobody（UID/GID 65534），清除附加组；
普通用户默认不降权。可用 `--uid`、`--gid`、`--no-drop-privileges` 控制。
降权后的用户必须能访问 cwd 和可执行文件。默认最小环境，`--inherit-env` 才继承全部环境。

## 结果与分类

```json
{
  "verdict": "OK",
  "cpu_time_us": 2500,
  "cpu_time_ms": 3,
  "real_time_ms": 4,
  "memory_kb": 2048,
  "memory_peak_bytes": 2097152,
  "rss_kb": 3072,
  "oom_events": 0,
  "oom_kills": 0,
  "timed_out": false,
  "signal": 0,
  "exit_code": 0,
  "message": "",
  "output_path": "/work/1.user.out",
  "stderr_path": "/work/1.user.out.err"
}
```

以上数值仅为字段示例。CPU 判断使用微秒，内存判断使用字节，避免展示时的取整影响
边界判定。`memory_kb` 是向上取整到 KiB 的 cgroup 峰值；`rss_kb` 是 `wait4` 的辅助统计。
`oom_events` 和 `oom_kills` 分别来自该次新建组的 `memory.events` 中 `oom` 和 `oom_kill`。

判定顺序：启动或管理失败 → SYSTEM_ERROR；OOM 证据 → MLE；wall 超时 → TLE；
内存峰值超题目阈值 → MLE；CPU 超题目阈值或 SIGXCPU → TLE；其他信号/非零退出 → RE；
否则 OK。多个原因同时发生时采用上述顺序。
即使程序捕获了分配失败，OOM 事件也不会消失。没有 OOM 证据时，不根据单独的 SIGKILL
或用户可控的 stderr 文案猜测 MLE。

CLI 退出码：OK 为 0，TLE/MLE/RE 为 1，SYSTEM_ERROR/参数错误为 2，中断为 130。
参数错误写 stderr，不产生 JSON。

## 读代码的顺序

1. `runner.py: run_case()`：准备参数，在 `with MemoryCgroup(...)` 内执行和取结果。
2. `memory_cgroup.py`：创建子组、设置保护上限、停止全部后代、读取峰值/OOM、删除子组。
3. `runner_helper.c: run_child()`：**先进入 cgroup，再 exec**，helper 和 Python 留在组外。
4. `monitor_child()`：一个 `wait4` 循环处理完成、wall 超时和取消。
5. `runner.py: _set_verdict()`：按原始题目阈值判定。

Python 仍先 exec 成小型 C helper，再由 helper fork 用户程序，避免辅助 RSS 统计受
Python 父进程大内存影响。cgroup 内存计量不依赖这个 RSS 修正，也不需要 `/proc` 采样。

正常、超时、setup 失败、Ctrl+C 都经过 cgroup 的退出清理。`cgroup.kill` 覆盖包括
`setsid` 后代在内的整个组，等待 `populated=0` 后才删除目录。每次创建新组，峰值与事件
自然从零开始，不需要重置历史计数。清理超时会报告错误并保留目录供排查。

这仍是资源执行器，不是完整安全沙箱；文件访问、网络以及阻止恶意操作自身权限可访问的
cgroup 需要外部隔离。CPU 仍由直接子进程的 `wait4`/`RLIMIT_CPU` 管理，不使用 cgroup CPU
配额，也不把它当成任意进程树的 CPU 总额。

## 其他参数

`--stack` 默认 64MiB，`--output-limit` 默认单文件 64MiB，`--nproc` 默认关闭。
始终禁用 core dump。CPU 和内存设为 0 时关闭自身对应保护上限；CPU 为 0 且未显式设置
wall 时也关闭自动 wall 限制。即使内存不限，仍建立 cgroup，用于统计和清理后代。
零值不会取消继承的外部限制；`--nproc` 是 UID 范围的限制，不是单提交进程配额。

## 验证

```bash
# 本机真实 cgroup 集成测试
systemd-run --user --scope -p Delegate=yes -- python3 examples/delegated.py make check

# 已有委派目录时
ROJ_JUDGE_CGROUP_ROOT=/path/to/delegated-parent make check
```

测试覆盖真实 OOM、保护余量内完成后再判超限、微秒/字节边界、瞬时峰值、多个后代的
内存计量、setsid 后代清理、Ctrl+C、setup 失败、以及执行后不遗留子组。
直接 `make check` 且未提供委派目录时，只执行纯逻辑测试，集成测试明确跳过。
降权测试需 root，普通用户运行时明确跳过。
