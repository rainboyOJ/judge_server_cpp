#include "ResultMapper.h"

#include <signal.h>

int map_monitor_result_to_judge_code(const judge_config &config,
                                     const monitor_result &monitor) {
    if (monitor.timed_out) {
        return REAL_TIME_LIMIT_EXCEEDED;
    }

    if (config.max_memory_bytes > 0 &&
        monitor.memory_bytes >
            static_cast<long long>(config.max_memory_bytes)) {
        return MEMORY_LIMIT_EXCEEDED;
    }

    if (config.max_memory_bytes > 0 &&
        (monitor.signal == SIGSEGV || monitor.signal == SIGABRT ||
         monitor.signal == SIGKILL)) {
        return MEMORY_LIMIT_EXCEEDED;
    }

    if (config.max_memory_bytes > 0 && monitor.exit_code == 127) {
        return MEMORY_LIMIT_EXCEEDED;
    }

    if (config.max_cpu_time_ms > 0 &&
        monitor.cpu_time_ms > static_cast<int>(config.max_cpu_time_ms)) {
        return CPU_TIME_LIMIT_EXCEEDED;
    }

    if (monitor.signal != 0 || monitor.exit_code != 0) {
        return RUNTIME_ERROR;
    }

    return SUCCESS;
}
