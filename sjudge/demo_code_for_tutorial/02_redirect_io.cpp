#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>

extern char **environ;
namespace fs = std::filesystem;

int main() {
    fs::create_directories("demo_tmp");
    {
        std::ofstream input("demo_tmp/case.in");
        input << "hello through stdin\n";
    }

    const pid_t pid = fork();
    if (pid == 0) {
        const int in = open("demo_tmp/case.in", O_RDONLY);
        const int out =
            open("demo_tmp/case.out", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (in < 0 || out < 0) {
            _exit(100);
        }
        if (dup2(in, STDIN_FILENO) < 0 || dup2(out, STDOUT_FILENO) < 0) {
            _exit(101);
        }
        close(in);
        close(out);

        char *const argv[] = {const_cast<char *>("/bin/cat"), nullptr};
        execve("/bin/cat", argv, environ);
        _exit(127);
    }

    if (pid < 0) {
        std::cerr << "fork failed\n";
        return 1;
    }

    int status = 0;
    waitpid(pid, &status, 0);
    std::cout << "wrote demo_tmp/case.out\n";
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
