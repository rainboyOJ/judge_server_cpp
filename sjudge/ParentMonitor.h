#pragma once

#include <sys/types.h>

#include <cstdint>

struct monitor_result {
    int wait_status{0};
    int signal{0};
    int exit_code{0};
    int real_time_ms{0};
    int cpu_time_ms{0};
    long long memory_bytes{0};
    bool timed_out{false};
    int error{0};
};

monitor_result monitor_child_process(pid_t pid, uint32_t real_time_limit_ms);
