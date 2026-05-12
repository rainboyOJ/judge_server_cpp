#include <cassert>

#include "sjudge_call.h"

namespace {

void test_invalid_config_returns_invalid_config_error() {
    judge_config config{};
    config.exe_path = "";
    config.max_cpu_time_ms = 10;

    const judge_result result = run_sjudger(config);

    assert(result.result == SYSTEM_ERROR || result.error == INVALID_CONFIG);
}

void test_missing_exe_is_invalid_config() {
    judge_config config{};
    config.exe_path.clear();

    const judge_result result = run_sjudger(config);

    assert(result.error == INVALID_CONFIG);
    assert(result.result == SYSTEM_ERROR);
}

void test_zero_limits_are_treated_as_unlimited_for_valid_exe() {
    judge_config config{};
    config.exe_path = "/bin/echo";
    config.args = {"/bin/echo", "ok"};
    config.max_cpu_time_ms = 0;
    config.max_real_time_ms = 0;
    config.max_memory_bytes = 0;
    config.max_stack_bytes = 0;
    config.max_output_bytes = 0;

    const judge_result result = run_sjudger(config);

    assert(result.error != INVALID_CONFIG);
}

} // namespace

int main() {
    test_invalid_config_returns_invalid_config_error();
    test_missing_exe_is_invalid_config();
    test_zero_limits_are_treated_as_unlimited_for_valid_exe();
    return 0;
}
