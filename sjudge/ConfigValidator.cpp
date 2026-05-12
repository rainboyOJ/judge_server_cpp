#include "ConfigValidator.h"
#include "sjudge_call.h"

bool validate_judge_config(const judge_config &config, int &error_code) {
    if (config.exe_path.empty()) {
        error_code = INVALID_CONFIG;
        return false;
    }
    error_code = 0;
    return true;
}
