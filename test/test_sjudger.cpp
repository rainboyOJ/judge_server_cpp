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

void test_zero_cpu_limit_is_treated_as_unlimited_but_missing_exe_is_invalid() {
    judge_config config{};
    config.max_cpu_time_ms = 0;
    config.max_real_time_ms = 0;
    config.max_memory_bytes = 0;
    config.exe_path.clear();

    const judge_result result = run_sjudger(config);

    assert(result.error == INVALID_CONFIG);
    assert(result.result == SYSTEM_ERROR);
}

void test_negative_like_limits_cannot_be_expressed_in_unsigned_fields() {
    judge_config config{};
    config.exe_path = "/bin/echo";
    config.args = {"/bin/echo", "ok"};

    const judge_result result = run_sjudger(config);

    assert(result.error != INVALID_CONFIG);
}

} // namespace

int main() {
    test_invalid_config_returns_invalid_config_error();
    test_zero_cpu_limit_is_treated_as_unlimited_but_missing_exe_is_invalid();
    test_negative_like_limits_cannot_be_expressed_in_unsigned_fields();
    return 0;
}
