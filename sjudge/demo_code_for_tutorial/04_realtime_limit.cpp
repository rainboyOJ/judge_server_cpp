#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <iostream>

extern char **environ;

int main() {
    constexpr int limit_ms = 500;
    const pid_t pid = fork();
    if (pid == 0) {
        char *const argv[] = {
            const_cast<char *>("/bin/sleep"),
            const_cast<char *>("10"),
            nullptr,
        };
        execve("/bin/sleep", argv, environ);
        _exit(127);
    }

    if (pid < 0) {
        std::cerr << "fork failed\n";
        return 1;
    }

    const auto start = std::chrono::steady_clock::now();
    int status = 0;
    bool timed_out = false;
    while (true) {
        const pid_t rc = waitpid(pid, &status, WNOHANG);
        if (rc == pid) {
            break;
        }
        if (rc < 0) {
            std::cerr << "waitpid failed\n";
            return 1;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
        if (elapsed > limit_ms) {
            timed_out = true;
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        usleep(1000);
    }

    std::cout << "timed_out=" << timed_out
              << " signal=" << (WIFSIGNALED(status) ? WTERMSIG(status) : 0)
              << "\n";
    return 0;
}
