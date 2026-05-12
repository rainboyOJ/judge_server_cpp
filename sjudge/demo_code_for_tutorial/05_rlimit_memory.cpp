#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

extern char **environ;

namespace {

int allocate_memory() {
    std::vector<std::string> blocks;
    while (true) {
        blocks.emplace_back(8 * 1024 * 1024, 'x');
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc > 1 && std::strcmp(argv[1], "--alloc-memory") == 0) {
        return allocate_memory();
    }

    const pid_t pid = fork();
    if (pid == 0) {
        constexpr rlim_t memory_limit = 64 * 1024 * 1024;
        rlimit limit{};
        limit.rlim_cur = memory_limit;
        limit.rlim_max = memory_limit;
        if (setrlimit(RLIMIT_AS, &limit) != 0) {
            _exit(100);
        }

        char *const child_argv[] = {
            argv[0],
            const_cast<char *>("--alloc-memory"),
            nullptr,
        };
        execve(argv[0], child_argv, environ);
        _exit(127);
    }

    if (pid < 0) {
        std::cerr << "fork failed\n";
        return 1;
    }

    int status = 0;
    rusage usage{};
    wait4(pid, &status, 0, &usage);

    std::cout << "exit_code=" << (WIFEXITED(status) ? WEXITSTATUS(status) : 0)
              << " signal=" << (WIFSIGNALED(status) ? WTERMSIG(status) : 0)
              << " maxrss_kb=" << usage.ru_maxrss << "\n";
    return 0;
}
