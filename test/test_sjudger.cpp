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

} // namespace

int main() {
    test_invalid_config_returns_invalid_config_error();
    return 0;
}
