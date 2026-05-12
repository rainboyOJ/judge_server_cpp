#include "ParentMonitor.h"

#include "sjudge_call.h"

#include <chrono>

#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using Clock = std::chrono::steady_clock;

/** @brief 把 POSIX timeval 转成毫秒，便于和题目限制字段比较。 */
int timeval_to_ms(const timeval &value) {
  return static_cast<int>(value.tv_sec * 1000 + value.tv_usec / 1000);
}

/** @brief 使用单调时钟统计真实时间，避免系统时间调整影响 TLE 判断。 */
int elapsed_ms_since(const Clock::time_point &start) {
  return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                              Clock::now() - start)
                              .count());
}

/**
 * @brief 从 wait4 的 status/rusage 填充统一监控结果。
 *
 * 这里保留原始 wait_status，同时抽取 runner 和 ResultMapper 关心的字段：
 * signal、exit_code、CPU 时间、真实时间、内存峰值。
 */
void fill_result_from_wait_status(monitor_result &result, int status,
                                  const rusage &usage,
                                  const Clock::time_point &start) {
  result.wait_status = status;
  result.signal = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
  result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
  result.cpu_time_ms = timeval_to_ms(usage.ru_utime);
  result.real_time_ms = elapsed_ms_since(start);
  result.memory_bytes = static_cast<long long>(usage.ru_maxrss) * 1024;
}

/** @brief 判断真实时间是否超过限制；0 表示不限制。 */
bool real_time_limit_exceeded(int elapsed_ms, uint32_t real_time_limit_ms) {
  return real_time_limit_ms > 0 &&
         elapsed_ms > static_cast<int>(real_time_limit_ms);
}

/**
 * @brief 子进程被父进程 kill 后，阻塞回收并采样最终状态。
 *
 * 超时路径必须 wait4 一次，否则会留下僵尸进程。极少数情况下 status 里
 * 可能没有 signal，这里补成 SIGKILL，保持 TLE 诊断信息稳定。
 */
bool collect_killed_child(pid_t pid, monitor_result &result,
                          const Clock::time_point &start) {
  int status = 0;
  rusage usage{};
  if (wait4(pid, &status, 0, &usage) < 0) {
    result.error = WAIT_FAILED;
    return false;
  }

  fill_result_from_wait_status(result, status, usage, start);
  if (result.signal == 0) {
    result.signal = SIGKILL;
  }
  return true;
}

} // namespace

monitor_result monitor_child_process(pid_t pid, uint32_t real_time_limit_ms) {
  monitor_result result{};
  const auto start = Clock::now();

  // 使用 WNOHANG 轮询，父进程才能同时检查真实时间上限。
  while (true) {
    int status = 0;
    rusage usage{};
    const pid_t wait_rc = wait4(pid, &status, WNOHANG, &usage);
    if (wait_rc == pid) {
      fill_result_from_wait_status(result, status, usage, start);
      break;
    }

    if (wait_rc < 0) {
      result.error = WAIT_FAILED;
      break;
    }

    result.real_time_ms = elapsed_ms_since(start);
    if (real_time_limit_exceeded(result.real_time_ms, real_time_limit_ms)) {
      result.timed_out = true;
      kill(pid, SIGKILL);
      collect_killed_child(pid, result, start);
      break;
    }

    usleep(1000);
  }

  return result;
}
