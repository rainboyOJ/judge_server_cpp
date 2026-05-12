#include "ChildSetup.h"
#include "ConfigValidator.h"
#include "ParentMonitor.h"
#include "ResultMapper.h"
#include "SJudger.h"

#include <unistd.h>

namespace {

bool is_child_setup_error_code(int exit_code) {
  switch (exit_code) {
  case SYSTEM_ERROR:
  case WAIT_FAILED:
  case INVALID_CONFIG:
  case FORK_FAILED:
  case LOAD_SECCOMP_FAILED:
  case SETRLIMIT_FAILED:
  case DUP2_FAILED:
  case SETUID_FAILED:
  case EXECVE_FAILED:
    return true;
  default:
    return false;
  }
}

} // namespace

judge_result run_sjudger(const judge_config &config) {
  judge_result result{};
  int error_code = 0;
  if (!validate_judge_config(config, error_code)) {
    result.result = SYSTEM_ERROR;
    result.error = error_code;
    return result;
  }

  const pid_t pid = fork();
  if (pid == 0) {
    run_child_process_or_exit(config);
  }
  if (pid < 0) {
    result.result = SYSTEM_ERROR;
    result.error = FORK_FAILED;
    return result;
  }

  const monitor_result monitor =
      monitor_child_process(pid, config.max_real_time_ms);
  result.cpu_time_ms = monitor.cpu_time_ms;
  result.real_time_ms = monitor.real_time_ms;
  result.memory_bytes = monitor.memory_bytes;
  result.signal = monitor.signal;
  result.exit_code = monitor.exit_code;

  if (monitor.error != 0) {
    result.result = SYSTEM_ERROR;
    result.error = monitor.error;
    return result;
  }

  if (result.signal == 0 && result.exit_code != 0 &&
      is_child_setup_error_code(result.exit_code)) {
    result.result = SYSTEM_ERROR;
    result.error = result.exit_code;
    return result;
  }

  result.error = 0;
  result.result = map_monitor_result_to_judge_code(config, monitor);
  return result;
}
