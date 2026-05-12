#include "ConfigValidator.h"

bool validate_judge_config(const judge_config &config, int &error_code) {
    if (config.exe_path.empty()) {
        error_code = INVALID_CONFIG;
        return false;
    }
    if (!config.max_cpu_time_ms && !config.max_real_time_ms &&
        !config.max_memory_bytes && !config.max_stack_bytes &&
        !config.max_output_bytes) {
        error_code = 0;
        return true;
    }
    error_code = 0;
    return true;
}
