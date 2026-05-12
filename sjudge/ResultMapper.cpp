#include "ResultMapper.h"

#include <signal.h>

int map_monitor_result_to_judge_code(const judge_config &config,
                                     const monitor_result &monitor) {
  // 超时优先级最高：真实时间超限通常表现为 SIGKILL，但语义应是 TLE。
  if (monitor.timed_out) {
    return REAL_TIME_LIMIT_EXCEEDED;
  }

  // ru_maxrss 在 Linux 上通常是 KB，ParentMonitor 已转换成 bytes。
  if (config.max_memory_bytes > 0 &&
      monitor.memory_bytes > static_cast<long long>(config.max_memory_bytes)) {
    return MEMORY_LIMIT_EXCEEDED;
  }

  // RLIMIT_CPU 触发时常见表现也是 SIGKILL/SIGXCPU；如果 CPU 时间已经超过
  // 配置上限，应先按 CPU 超限处理，避免被下面的内存信号兜底误判成 MLE。
  if (config.max_cpu_time_ms > 0 &&
      monitor.cpu_time_ms > static_cast<int>(config.max_cpu_time_ms)) {
    return CPU_TIME_LIMIT_EXCEEDED;
  }

  // 内存限制可能让程序在 RSS 超过上限前就异常终止，尤其是解释器或 allocator。
  if (config.max_memory_bytes > 0 &&
      (monitor.signal == SIGSEGV || monitor.signal == SIGABRT ||
       monitor.signal == SIGKILL)) {
    return MEMORY_LIMIT_EXCEEDED;
  }

  // Python/sh wrapper 在极低 RLIMIT_AS 下可能以 127 结束，按 MLE 兜底处理。
  if (config.max_memory_bytes > 0 && monitor.exit_code == 127) {
    return MEMORY_LIMIT_EXCEEDED;
  }

  // 剩余非零退出或异常信号，才是普通运行时错误。
  if (monitor.signal != 0 || monitor.exit_code != 0) {
    return RUNTIME_ERROR;
  }

  return SUCCESS;
}
