#include "ParentMonitor.h"

#include "sjudge_call.h"

#include <chrono>

#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

using Clock = std::chrono::steady_clock;

int timeval_to_ms(const timeval &value) {
  return static_cast<int>(value.tv_sec * 1000 + value.tv_usec / 1000);
}

int elapsed_ms_since(const Clock::time_point &start) {
  return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                              Clock::now() - start)
                              .count());
}

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

bool real_time_limit_exceeded(int elapsed_ms, uint32_t real_time_limit_ms) {
  return real_time_limit_ms > 0 &&
         elapsed_ms > static_cast<int>(real_time_limit_ms);
}

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
