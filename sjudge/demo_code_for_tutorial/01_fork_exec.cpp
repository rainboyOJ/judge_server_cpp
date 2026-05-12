#include <sys/wait.h>
#include <unistd.h>

#include <iostream>

extern char **environ;

int main() {
    const pid_t pid = fork();
    if (pid == 0) {
        char *const argv[] = {
            const_cast<char *>("/bin/echo"),
            const_cast<char *>("hello from child"),
            nullptr,
        };
        execve("/bin/echo", argv, environ);
        _exit(127);
    }

    if (pid < 0) {
        std::cerr << "fork failed\n";
        return 1;
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        std::cout << "child exit_code=" << WEXITSTATUS(status) << "\n";
    }
    if (WIFSIGNALED(status)) {
        std::cout << "child signal=" << WTERMSIG(status) << "\n";
    }
    return 0;
}
