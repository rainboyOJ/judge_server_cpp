#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>

extern char **environ;

namespace {

int timeval_to_ms(const timeval &value) {
    return static_cast<int>(value.tv_sec * 1000 + value.tv_usec / 1000);
}

} // namespace

int main() {
    const pid_t pid = fork();
    if (pid == 0) {
        char *const argv[] = {
            const_cast<char *>("/bin/sh"),
            const_cast<char *>("-c"),
            const_cast<char *>("i=0; while [ $i -lt 200000 ]; do i=$((i+1)); done"),
            nullptr,
        };
        execve("/bin/sh", argv, environ);
        _exit(127);
    }

    if (pid < 0) {
        std::cerr << "fork failed\n";
        return 1;
    }

    int status = 0;
    rusage usage{};
    wait4(pid, &status, 0, &usage);

    std::cout << "exited=" << WIFEXITED(status)
              << " exit_code=" << (WIFEXITED(status) ? WEXITSTATUS(status) : 0)
              << " signaled=" << WIFSIGNALED(status)
              << " signal=" << (WIFSIGNALED(status) ? WTERMSIG(status) : 0)
              << " cpu_ms=" << timeval_to_ms(usage.ru_utime)
              << " maxrss_kb=" << usage.ru_maxrss << "\n";
    return 0;
}
