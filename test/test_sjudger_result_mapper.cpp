#include <cassert>
#include <signal.h>

#include "ResultMapper.h"

namespace {

judge_config make_config() {
  judge_config config{};
  config.exe_path = "/bin/true";
  return config;
}

void test_timed_out_maps_to_real_time_limit() {
  const judge_config config = make_config();
  monitor_result monitor{};
  monitor.timed_out = true;
  monitor.signal = SIGKILL;

  assert(map_monitor_result_to_judge_code(config, monitor) ==
         REAL_TIME_LIMIT_EXCEEDED);
}

void test_memory_peak_maps_to_mle() {
  judge_config config = make_config();
  config.max_memory_bytes = 1024;
  monitor_result monitor{};
  monitor.memory_bytes = 2048;

  assert(map_monitor_result_to_judge_code(config, monitor) ==
         MEMORY_LIMIT_EXCEEDED);
}

void test_memory_signal_maps_to_mle_when_memory_limit_is_set() {
  judge_config config = make_config();
  config.max_memory_bytes = 64 * 1024 * 1024;
  monitor_result monitor{};
  monitor.signal = SIGABRT;

  assert(map_monitor_result_to_judge_code(config, monitor) ==
         MEMORY_LIMIT_EXCEEDED);
}

void test_cpu_time_maps_to_cpu_time_limit() {
  judge_config config = make_config();
  config.max_cpu_time_ms = 10;
  monitor_result monitor{};
  monitor.cpu_time_ms = 20;

  assert(map_monitor_result_to_judge_code(config, monitor) ==
         CPU_TIME_LIMIT_EXCEEDED);
}

void test_cpu_time_takes_precedence_over_memory_signal_fallback() {
  judge_config config = make_config();
  config.max_cpu_time_ms = 10;
  config.max_memory_bytes = 64 * 1024 * 1024;
  monitor_result monitor{};
  monitor.cpu_time_ms = 1000;
  monitor.memory_bytes = 1024 * 1024;
  monitor.signal = SIGKILL;

  assert(map_monitor_result_to_judge_code(config, monitor) ==
         CPU_TIME_LIMIT_EXCEEDED);
}

void test_non_zero_exit_maps_to_runtime_error() {
  const judge_config config = make_config();
  monitor_result monitor{};
  monitor.exit_code = 3;

  assert(map_monitor_result_to_judge_code(config, monitor) == RUNTIME_ERROR);
}

void test_clean_exit_maps_to_success() {
  const judge_config config = make_config();
  monitor_result monitor{};

  assert(map_monitor_result_to_judge_code(config, monitor) == SUCCESS);
}

} // namespace

int main() {
  test_timed_out_maps_to_real_time_limit();
  test_memory_peak_maps_to_mle();
  test_memory_signal_maps_to_mle_when_memory_limit_is_set();
  test_cpu_time_maps_to_cpu_time_limit();
  test_cpu_time_takes_precedence_over_memory_signal_fallback();
  test_non_zero_exit_maps_to_runtime_error();
  test_clean_exit_maps_to_success();
  return 0;
}
