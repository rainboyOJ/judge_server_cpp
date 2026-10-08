#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""本地评测工具：编译提交、跑 testData/<pid>/data 的测试点、比对答案并汇总结果。

    python3 local_judge.py --pid 1000 solution.cpp
    python3 local_judge.py --pid 1000 solution.py --testdata ../testData

它把三件事串起来，让用户不用起 judge_server 就能自己验一份代码：

1. 编译：C++ 用和 judge_server 相同的 `g++ -std=c++17 -O2 -DONLINE_JUDGE`；
   Python 用 `py_compile` 做语法检查，把解释型语言统一映射到“编译阶段”。
2. 执行：优先用 cgroup v2 做内存/CPU 隔离；不可用时自动降级为 wall 限制加
   `RLIMIT_CPU`，并明确提示 MLE 无法判定。
3. 比对：优先用 `/judge/checker/fcmp2`，否则按行比较，忽略行尾空白和末尾空行
   —— 与 judge_server 的 fallback 路径同一套规则。

限制与判定口径来自 `runner.py`，本文件只负责串流程和展示。它不替代服务端判题，
详细差异见本目录 README.md 的“与 judge_server 的差异”一节。
"""

from __future__ import annotations

import argparse
import json
import os
import re
import resource
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Optional

# 同目录导入：允许直接 `python3 local_judge.py` 而不需要安装成包。
sys.path.insert(0, str(Path(__file__).resolve().parent))

# 复用 runner.py 的判定与子进程环境，避免两处各维护一份口径。
from runner import CaseResult, Limits, Verdict, _child_env, _set_verdict, run_case

PACKAGE_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = PACKAGE_DIR.parent
DEFAULT_TESTDATA = PROJECT_ROOT / "testData"
DEFAULT_CGROUP_ROOT = Path("/sys/fs/cgroup/roj-judge")
CHECKER_PATH = Path("/judge/checker/fcmp2")
COMPILE_TIMEOUT_S = 120

LANG_BY_SUFFIX = {
    ".cpp": "cpp",
    ".cc": "cpp",
    ".cxx": "cpp",
    ".py": "python",
}


# --------------------------------------------------------------------------
# 测试点发现与题目限制
# --------------------------------------------------------------------------


def _natural_key(name: str) -> list[tuple[int, object]]:
    """让 problem2 排在 problem10 前面；同时保证不同名字之间始终可比较。"""
    parts = re.split(r"(\d+)", name)
    return [(0, int(part)) if part.isdigit() else (1, part) for part in parts]


def load_cases(data_dir: Path) -> list[tuple[str, Path, Path]]:
    """扫描 <pid>/data，按 judge_server 的规则配对 .in/.out。"""
    cases: list[tuple[str, Path, Path]] = []
    for in_path in data_dir.iterdir():
        if not in_path.is_file() or in_path.suffix != ".in":
            continue
        out_path = in_path.with_suffix(".out")
        if out_path.exists():
            cases.append((in_path.stem, in_path, out_path))
    return sorted(cases, key=lambda case: _natural_key(case[0]))


def load_problem_meta(problem_dir: Path) -> tuple[str, int, int]:
    """读题目 config.json 的标题、CPU 限制 ms 和内存限制 MiB。

    judge_server 目前忽略这个文件（固定 1000ms / 1GiB），本地工具按题目声明的
    限制评测更贴近用户预期，也便于用 --time / --memory 覆盖。
    """
    title, time_ms, memory_mb = "", 1000, 128
    config_path = problem_dir / "config.json"
    if not config_path.is_file():
        return title, time_ms, memory_mb
    try:
        data = json.loads(config_path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return title, time_ms, memory_mb
    if isinstance(data.get("title"), str):
        title = data["title"]
    for key, default in (("time", time_ms), ("memory", memory_mb)):
        value = data.get(key)
        if isinstance(value, int) and value > 0:
            if key == "time":
                time_ms = value
            else:
                memory_mb = value
    return title, time_ms, memory_mb


# --------------------------------------------------------------------------
# 编译
# --------------------------------------------------------------------------


def compile_submission(lang: str, source: Path, work_dir: Path) -> tuple[Optional[list[str]], str]:
    """返回 (运行命令, 编译输出)。命令为 None 表示编译失败（CE）。"""
    if lang == "cpp":
        executable = work_dir / "solution"
        # 与 judge_server 的 RunnerCompileSupport.cpp 保持同一组参数。
        command = ["g++", "-std=c++17", "-O2", "-DONLINE_JUDGE",
                   "-o", str(executable), str(source)]
        try:
            proc = subprocess.run(command, capture_output=True, text=True,
                                  timeout=COMPILE_TIMEOUT_S)
        except FileNotFoundError:
            return None, "找不到 g++，请先安装 C++ 编译器"
        except subprocess.TimeoutExpired:
            return None, f"编译超过 {COMPILE_TIMEOUT_S}s 未结束，已终止"
        if proc.returncode != 0 or not executable.is_file():
            return None, (proc.stderr or proc.stdout).strip()
        return [str(executable)], ""

    if lang == "python":
        try:
            proc = subprocess.run([sys.executable, "-m", "py_compile", str(source)],
                                  capture_output=True, text=True, timeout=COMPILE_TIMEOUT_S)
        except FileNotFoundError:
            return None, f"找不到 Python 解释器: {sys.executable}"
        except subprocess.TimeoutExpired:
            return None, f"语法检查超过 {COMPILE_TIMEOUT_S}s 未结束，已终止"
        if proc.returncode != 0:
            return None, (proc.stderr or proc.stdout).strip()
        return ["python3", str(source)], ""

    return None, f"不支持的语言: {lang}"


def detect_language(source: Path, requested: Optional[str]) -> tuple[Optional[str], str]:
    if requested and requested != "auto":
        if requested not in ("cpp", "python"):
            return None, f"未知语言 {requested}，只支持 cpp 和 python"
        return requested, ""
    lang = LANG_BY_SUFFIX.get(source.suffix.lower())
    if lang:
        return lang, ""
    if source.suffix.lower() == ".c":
        return None, "judge_server 当前不支持 C 语言 runner，请改用 .cpp"
    return None, f"无法从后缀 {source.suffix!r} 判断语言，请用 --lang 指定"


# --------------------------------------------------------------------------
# 输出比对
# --------------------------------------------------------------------------


def normalize_lines(text: str) -> list[str]:
    """忽略行尾空白和末尾空行，与 RunnerExecutionSupport.cpp 一致。"""
    lines = [line.rstrip(" \t\r") for line in text.splitlines()]
    while lines and not lines[-1]:
        lines.pop()
    return lines


def _read_text(path: Path) -> str:
    return path.read_bytes().decode("utf-8", errors="replace")


def compare_output(input_path: Path, expected_path: Path, user_output: Path,
                   checker: Optional[Path]) -> bool:
    if checker is not None:
        # fcmp2 的调用约定与 judge_server 相同：<输入> <用户输出> <标准答案>。
        proc = subprocess.run([str(checker), str(input_path), str(user_output),
                               str(expected_path)], capture_output=True, text=True)
        return proc.returncode == 0
    return normalize_lines(_read_text(expected_path)) == normalize_lines(_read_text(user_output))


def _clip(text: Optional[str], limit: int = 60) -> str:
    if text is None:
        return "<无此行>"
    return text if len(text) <= limit else text[:limit] + "…"


def first_difference(expected_path: Path, user_output: Path) -> str:
    """给 WA 找第一条不同的行，方便用户直接定位。"""
    expected = normalize_lines(_read_text(expected_path))
    actual = normalize_lines(_read_text(user_output))
    for index in range(max(len(expected), len(actual))):
        want = expected[index] if index < len(expected) else None
        got = actual[index] if index < len(actual) else None
        if want != got:
            return f"第 {index + 1} 行：期望 {_clip(want)}，实际 {_clip(got)}"
    return ""


# --------------------------------------------------------------------------
# 降级执行（没有 cgroup 时）
# --------------------------------------------------------------------------


def _apply_rlimits(limits: Limits) -> None:
    """在 exec 前设置内核级兜底限制；内存不做限制，因为 RSS 口径会误判。"""
    cpu_seconds = max(1, (limits.time_ms + limits.cpu_slack_ms + 999) // 1000) if limits.time_ms else 0
    if cpu_seconds:
        resource.setrlimit(resource.RLIMIT_CPU, (cpu_seconds, cpu_seconds + 1))
    if limits.stack_mb:
        stack_bytes = limits.stack_mb * 1024 * 1024
        soft, hard = resource.getrlimit(resource.RLIMIT_STACK)
        resource.setrlimit(resource.RLIMIT_STACK, (stack_bytes, min(hard, max(soft, stack_bytes))))
    if limits.output_limit_mb:
        size = limits.output_limit_mb * 1024 * 1024
        resource.setrlimit(resource.RLIMIT_FSIZE, (size, size))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


def run_case_degraded(argv: list[str], input_path: Path, output_path: Path,
                      limits: Limits, cwd: Path, stderr_path: Path) -> CaseResult:
    """没有可用 cgroup 时的降级执行：只强制 wall 超时和 RLIMIT_CPU。

    不使用 RSS 猜测 MLE，所以这里只报告 rss_kb 作诊断。判定仍交给 runner 的
    `_set_verdict`，保证与隔离模式同一套规则。
    """
    result = CaseResult()
    wall_ms = limits.resolved_wall_ms()
    start = time.monotonic()
    try:
        with open(input_path, "rb") as fin, open(output_path, "wb") as fout, \
                open(stderr_path, "wb") as ferr:
            process = subprocess.Popen(
                argv, stdin=fin, stdout=fout, stderr=ferr, cwd=str(cwd),
                env=_child_env(cwd, False), start_new_session=True,
                preexec_fn=lambda: _apply_rlimits(limits),
            )
            deadline = start + wall_ms / 1000 if wall_ms else None
            timed_out = False
            while True:
                pid, status, usage = os.wait4(process.pid, os.WNOHANG)
                if pid:
                    break
                if deadline is not None and time.monotonic() >= deadline:
                    timed_out = True
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    _, status, usage = os.wait4(process.pid, 0)
                    break
                time.sleep(0.002)
            # 已经直接 reap，同步回 Popen 状态，避免它再次 wait。
            process.returncode = status
    except OSError as exc:
        result.message = f"启动失败: {exc}"
        return result

    result.cpu_time_us = int((usage.ru_utime + usage.ru_stime) * 1_000_000)
    result.cpu_time_ms = round(result.cpu_time_us / 1000)
    result.real_time_ms = round((time.monotonic() - start) * 1000)
    result.rss_kb = usage.ru_maxrss
    result.timed_out = timed_out
    if os.WIFSIGNALED(status):
        result.signal = os.WTERMSIG(status)
    elif os.WIFEXITED(status):
        result.exit_code = os.WEXITSTATUS(status)
    _set_verdict(result, limits)
    return result


# --------------------------------------------------------------------------
# cgroup 可用性与自动委派
# --------------------------------------------------------------------------


def check_cgroup_root(root: Path) -> tuple[bool, str]:
    """确认父目录存在、已为子组启用 memory controller，并且真的能建子组。"""
    if not root.is_dir():
        return False, f"{root} 不存在"
    try:
        enabled = (root / "cgroup.subtree_control").read_text().split()
    except OSError as exc:
        return False, f"无法读取 {root}/cgroup.subtree_control：{exc}"
    if "memory" not in enabled:
        return False, f"{root} 未为子组启用 memory controller"
    probe = root / f"local-judge-probe-{os.getpid()}"
    try:
        probe.mkdir()
        probe.rmdir()
    except OSError as exc:
        return False, f"{root} 不可写：{exc}"
    return True, ""


def try_auto_delegate() -> tuple[bool, str]:
    """探测能否用 systemd-run 起一个委派 scope；成功则重新执行自己。

    这样普通用户不必手工拼 `systemd-run --user --scope -p Delegate=yes`，
    也避免在容器等没有 user manager 的环境里报错退出。
    """
    if not shutil.which("systemd-run"):
        return False, "未找到 systemd-run"
    if not os.environ.get("XDG_RUNTIME_DIR"):
        return False, "缺少 XDG_RUNTIME_DIR"
    probe = [*_delegated_prefix(), sys.executable, "-c", "print('local-judge-delegated-ok')"]
    try:
        proc = subprocess.run(probe, capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired) as exc:
        return False, f"systemd-run 探测失败：{exc}"
    if proc.returncode == 0 and "local-judge-delegated-ok" in proc.stdout:
        return True, ""
    detail = (proc.stderr or proc.stdout).strip().splitlines()
    return False, detail[-1] if detail else f"systemd-run 退出码 {proc.returncode}"


def _delegated_prefix() -> list[str]:
    """构造 `systemd-run ... delegated.py <命令>` 的前缀。

    --quiet 去掉 systemd-run 自己的 “Running as unit” 提示，但保留被测命令的
    stdout/stderr；--scope 让命令同步执行并原样传回退出码。
    """
    return ["systemd-run", "--user", "--quiet", "--scope", "-p", "Delegate=yes", "--",
            sys.executable, str(PACKAGE_DIR / "examples" / "delegated.py")]


def run_delegated(raw_args: list[str]) -> int:
    """在委派 scope 里重跑自己，返回子进程退出码。

    delegated.py 会设置 ROJ_JUDGE_CGROUP_ROOT，所以子进程直接走隔离路径；
    追加 --no-delegate 防止它在子进程里再次尝试委派。
    """
    command = [*_delegated_prefix(), sys.executable, str(Path(__file__).resolve()),
               *raw_args, "--no-delegate"]
    try:
        return subprocess.call(command)
    except OSError as exc:
        print(f"无法在委派 scope 中启动评测：{exc}", file=sys.stderr)
        return 2


# --------------------------------------------------------------------------
# 展示
# --------------------------------------------------------------------------


def format_case_line(index: int, name: str, verdict: str, result: CaseResult,
                     detail: str) -> str:
    timing = f"{result.cpu_time_ms:>5}ms {result.memory_kb / 1024:>6.1f}MiB"
    line = f"  #{index:<3} {name:<12} {verdict:<6} {timing}"
    return f"{line}   {detail}" if detail else line


def describe(result: CaseResult, verdict: str, expected_path: Path,
             user_output: Path) -> str:
    if verdict == "AC":
        return ""
    if verdict == "WA":
        return first_difference(expected_path, user_output)
    if verdict == "TLE":
        wall = "wall 超时，" if result.timed_out else ""
        return f"{wall}CPU {result.cpu_time_ms}ms / 实际 {result.real_time_ms}ms"
    if verdict == "MLE":
        return f"内存峰值 {result.memory_kb / 1024:.1f}MiB"
    if verdict == "RE":
        if result.signal:
            return f"被信号 {result.signal} 终止"
        return f"退出码 {result.exit_code}"
    return result.message


def list_problems(testdata_root: Path) -> int:
    if not testdata_root.is_dir():
        print(f"测试数据目录不存在：{testdata_root}", file=sys.stderr)
        return 2
    print(f"测试数据目录：{testdata_root}")
    for problem_dir in sorted(testdata_root.iterdir(), key=lambda p: _natural_key(p.name)):
        if not problem_dir.is_dir():
            continue
        data_dir = problem_dir / "data"
        count = len(load_cases(data_dir)) if data_dir.is_dir() else 0
        title, time_ms, memory_mb = load_problem_meta(problem_dir)
        print(f"  {problem_dir.name:<8} {count:>3} 个测试点  {time_ms}ms / {memory_mb}MiB  {title}")
    return 0


# --------------------------------------------------------------------------
# 主流程
# --------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="local_judge.py",
        description="本地评测：编译提交、跑测试点、比对答案（不需要 judge_server）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""示例：
  python3 local_judge.py --pid 1000 solution.cpp
  python3 local_judge.py --list
  python3 local_judge.py --pid 1000 solution.py --time 2000 --memory 256
""")
    parser.add_argument("source", nargs="?", type=Path, help="提交源文件（.cpp 或 .py）")
    parser.add_argument("--pid", help="题目编号，对应 testData/<pid>/")
    parser.add_argument("--lang", choices=("auto", "cpp", "python"), default="auto",
                        help="提交语言，默认按后缀判断")
    parser.add_argument("--testdata", type=Path, default=DEFAULT_TESTDATA,
                        help=f"测试数据根目录，默认 {DEFAULT_TESTDATA}")
    parser.add_argument("--time", type=int, help="CPU 限制 ms，覆盖题目 config.json")
    parser.add_argument("--memory", type=int, help="内存限制 MiB，覆盖题目 config.json")
    parser.add_argument("--checker", default="auto",
                        help="输出比较器；auto 表示存在 /judge/checker/fcmp2 就用它，none 强制按行比较")
    parser.add_argument("--cgroup-root", type=Path, default=None,
                        help=f"已委派的 cgroup v2 父目录，默认 $ROJ_JUDGE_CGROUP_ROOT 或 {DEFAULT_CGROUP_ROOT}")
    parser.add_argument("--delegate", choices=("auto", "never", "always"), default="auto",
                        help="cgroup 不可用时是否自动用 systemd-run 起委派 scope")
    parser.add_argument("--no-delegate", dest="delegate", action="store_const", const="never",
                        help="内部使用：禁止再次自动委派")
    parser.add_argument("--no-cgroup", action="store_true",
                        help="跳过 cgroup，直接用降级模式（只限 wall 和 CPU）")
    parser.add_argument("--keep-work-dir", action="store_true",
                        help="保留临时工作目录，便于查看输出和编译日志")
    parser.add_argument("--list", action="store_true", help="列出 testData 下可用的题目后退出")
    return parser


def resolve_checker(spec: str) -> tuple[Optional[Path], str]:
    if spec == "none":
        return None, "内置按行比较（忽略行尾空白和末尾空行）"
    if spec == "auto":
        if CHECKER_PATH.is_file():
            return CHECKER_PATH, f"{CHECKER_PATH}（与 judge_server 优先路径一致）"
        return None, "内置按行比较（未找到 /judge/checker/fcmp2）"
    path = Path(spec)
    if not path.is_file():
        raise SystemExit(f"指定的比较器不存在：{path}")
    return path, str(path)


def main(argv: Optional[list[str]] = None) -> int:
    raw_args = list(sys.argv[1:] if argv is None else argv)
    parser = build_parser()
    args = parser.parse_args(argv)

    testdata_root = Path(args.testdata).resolve()
    if args.list:
        return list_problems(testdata_root)

    if not args.pid:
        parser.error("请用 --pid 指定题目编号，或先用 --list 查看可用题目")
    if args.source is None:
        parser.error("请提供提交源文件，例如：python3 local_judge.py --pid 1000 solution.cpp")

    source = args.source.resolve()
    if not source.is_file():
        print(f"提交文件不存在：{source}", file=sys.stderr)
        return 2
    lang, error = detect_language(source, args.lang)
    if lang is None:
        print(error, file=sys.stderr)
        return 2

    problem_dir = testdata_root / args.pid
    data_dir = problem_dir / "data"
    if not data_dir.is_dir():
        print(f"找不到测试数据：{data_dir}", file=sys.stderr)
        print(f"可用题目：{', '.join(p.name for p in sorted(testdata_root.iterdir()))}"
              if testdata_root.is_dir() else f"测试数据目录不存在：{testdata_root}", file=sys.stderr)
        return 2
    cases = load_cases(data_dir)
    if not cases:
        print(f"{data_dir} 下没有配对的 .in/.out 测试点", file=sys.stderr)
        return 2

    title, time_ms, memory_mb = load_problem_meta(problem_dir)
    if args.time is not None:
        time_ms = args.time
    if args.memory is not None:
        memory_mb = args.memory
    limits = Limits(time_ms=time_ms, memory_kb=memory_mb * 1024)

    checker, checker_note = resolve_checker(args.checker)

    # 决定执行模式：cgroup 隔离 -> 自动委派重跑 -> 降级执行。
    cgroup_root = Path(args.cgroup_root or os.environ.get("ROJ_JUDGE_CGROUP_ROOT", DEFAULT_CGROUP_ROOT))
    isolated, reason = (False, "已用 --no-cgroup 禁用") if args.no_cgroup else check_cgroup_root(cgroup_root)
    # 显式传 --cgroup-root 时尊重用户的选择，不再自动换一个 scope 去委派。
    may_delegate = (not isolated and not args.no_cgroup
                    and args.cgroup_root is None and args.delegate != "never")
    if may_delegate:
        delegated, delegate_reason = try_auto_delegate()
        if delegated:
            # 交给子进程完整跑一遍并打印结果；本进程只转发退出码。
            return run_delegated(raw_args)
        if args.delegate == "always":
            print(f"无法建立 cgroup 委派：{delegate_reason}", file=sys.stderr)
            return 2
        reason = f"{reason}；自动委派不可用（{delegate_reason}）"

    print(f"题目 {args.pid}" + (f"  {title}" if title else ""))
    print(f"提交 {source.name}（{lang}）")
    print(f"限制 CPU {time_ms}ms / 内存 {memory_mb}MiB / wall {limits.resolved_wall_ms()}ms")
    if isolated:
        print(f"执行 cgroup 隔离，root={cgroup_root}")
    else:
        print(f"执行降级模式：{reason}")
        print("  ⚠ 无内存隔离，MLE 无法判定；TLE 依赖 wall 与 RLIMIT_CPU（整秒）")
    print(f"比较 {checker_note}")
    print()

    work_dir = Path(tempfile.mkdtemp(prefix="local-judge-"))
    if os.geteuid() == 0:
        # 降权后运行的提交需要能进入工作目录；judge_server 同样要处理这一点。
        os.chmod(work_dir, 0o755)

    try:
        run_argv, compile_output = compile_submission(lang, source, work_dir)
        if run_argv is None:
            print("编译失败（CE）：")
            print(compile_output or "（编译器没有输出）")
            return 2
        print("编译通过")

        started = time.monotonic()
        verdicts: list[str] = []
        for index, (name, input_path, expected_path) in enumerate(cases, start=1):
            user_output = work_dir / f"case-{index}.out"
            stderr_path = work_dir / f"case-{index}.err"
            if isolated:
                result = run_case(
                    run_argv, input_path, user_output, limits,
                    stderr_path=stderr_path, cwd=work_dir,
                    helper_path=PACKAGE_DIR / "runner_helper",
                    cgroup_root=cgroup_root,
                )
            else:
                result = run_case_degraded(run_argv, input_path, user_output,
                                           limits, work_dir, stderr_path)

            verdict = result.verdict.value
            if verdict == "OK":
                verdict = "AC" if compare_output(input_path, expected_path,
                                                 user_output, checker) else "WA"
            elif verdict == "SYSTEM_ERROR":
                verdict = "SE"
            verdicts.append(verdict)
            detail = describe(result, verdict, expected_path, user_output)
            if verdict == "SE":
                detail = result.message
            print(format_case_line(index, name, verdict, result, detail))

        elapsed = time.monotonic() - started
        passed = verdicts.count("AC")
        # 汇总取第一个非 AC 的结果，便于一眼看到“卡在哪一步”。
        overall = next((v for v in verdicts if v != "AC"), "AC")
        print()
        print(f"结果：{overall}  通过 {passed}/{len(cases)}  用时 {elapsed:.2f}s")
        if args.keep_work_dir:
            print(f"工作目录：{work_dir}")
        return 0 if overall == "AC" else 1
    finally:
        if not args.keep_work_dir:
            shutil.rmtree(work_dir, ignore_errors=True)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(130)
