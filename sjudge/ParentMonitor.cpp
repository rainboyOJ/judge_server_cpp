#include "ParentMonitor.h"

#include "sjudge_call.h"

#include <chrono>

#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

int timeval_to_ms(const timeval &value) {
    return static_cast<int>(value.tv_sec * 1000 + value.tv_usec / 1000);
}

} // namespace

monitor_result monitor_child_process(pid_t pid, uint32_t real_time_limit_ms) {
    monitor_result result{};
    const auto start = std::chrono::steady_clock::now();

    while (true) {
        int status = 0;
        rusage usage{};
        const pid_t wait_rc = wait4(pid, &status, WNOHANG, &usage);
        if (wait_rc == pid) {
            result.wait_status = status;
            result.signal = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
            result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
            result.cpu_time_ms = timeval_to_ms(usage.ru_utime);
            result.real_time_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count());
            result.memory_bytes = static_cast<long long>(usage.ru_maxrss) * 1024;
            break;
        }

        if (wait_rc < 0) {
            result.error = WAIT_FAILED;
            break;
        }

        result.real_time_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start)
                .count());
        if (real_time_limit_ms > 0 &&
            result.real_time_ms > static_cast<int>(real_time_limit_ms)) {
            result.timed_out = true;
            kill(pid, SIGKILL);

            status = 0;
            usage = {};
            if (wait4(pid, &status, 0, &usage) < 0) {
                result.error = WAIT_FAILED;
                break;
            }

            result.wait_status = status;
            result.signal = WIFSIGNALED(status) ? WTERMSIG(status) : SIGKILL;
            result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
            result.cpu_time_ms = timeval_to_ms(usage.ru_utime);
            result.real_time_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count());
            result.memory_bytes = static_cast<long long>(usage.ru_maxrss) * 1024;
            break;
        }

        usleep(1000);
    }

    return result;
}
