#pragma once

#include <sys/types.h>

#include <cstdint>

/**
 * @brief 父进程监控子进程后得到的原始状态。
 *
 * 这个结构仍然接近 POSIX wait/rusage 语义，不直接表达 AC/TLE/MLE/RE。
 * 稳定 verdict 由 `ResultMapper` 负责。
 */
struct monitor_result {
  /** @brief wait4 返回的原始 status。 */
  int wait_status{0};
  /** @brief 子进程被信号终止时的信号值；正常退出为 0。 */
  int signal{0};
  /** @brief 子进程正常退出时的 exit code。 */
  int exit_code{0};
  /** @brief 父进程观测到的真实时间，单位毫秒。 */
  int real_time_ms{0};
  /** @brief rusage 中的用户态 CPU 时间，单位毫秒。 */
  int cpu_time_ms{0};
  /** @brief rusage 中的内存峰值，单位字节。 */
  long long memory_bytes{0};
  /** @brief 是否由父进程因为真实时间超限而杀死。 */
  bool timed_out{false};
  /** @brief 监控过程中的基础设施错误码；无错误为 0。 */
  int error{0};
};

/**
 * @brief 在父进程中等待并监控子进程。
 *
 * 使用 `wait4(WNOHANG)` 轮询子进程状态，同时用单调时钟统计真实时间。
 * 当 `real_time_limit_ms` 非 0 且超限时，父进程发送 SIGKILL 并回收子进程。
 *
 * @param pid fork 得到的子进程 pid。
 * @param real_time_limit_ms 真实时间限制，毫秒；0 表示不限制。
 * @return monitor_result 原始监控结果。
 */
monitor_result monitor_child_process(pid_t pid, uint32_t real_time_limit_ms);
