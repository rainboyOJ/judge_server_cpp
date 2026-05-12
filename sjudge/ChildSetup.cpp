#include "ChildSetup.h"

#include <fcntl.h>
#include <unistd.h>

#include <string>
#include <vector>

extern char **environ;

namespace {

void redirect_fd(const std::string &path, int open_flags, int target_fd) {
    const int fd = open(path.c_str(), open_flags, 0644);
    if (fd < 0 || dup2(fd, target_fd) < 0) {
        _exit(DUP2_FAILED);
    }
    close(fd);
}

} // namespace

[[noreturn]] void run_child_process_or_exit(const judge_config &config) {
    if (!config.cwd.empty() && chdir(config.cwd.c_str()) != 0) {
        _exit(SYSTEM_ERROR);
    }

    redirect_fd(config.input_path, O_RDONLY, STDIN_FILENO);
    redirect_fd(config.output_path, O_WRONLY | O_CREAT | O_TRUNC, STDOUT_FILENO);
    redirect_fd(config.error_path, O_WRONLY | O_CREAT | O_TRUNC, STDERR_FILENO);

    std::vector<char *> argv;
    for (const std::string &arg : config.args) {
        argv.push_back(const_cast<char *>(arg.c_str()));
    }
    if (argv.empty()) {
        argv.push_back(const_cast<char *>(config.exe_path.c_str()));
    }
    argv.push_back(nullptr);

    execve(config.exe_path.c_str(), argv.data(), environ);
    _exit(EXECVE_FAILED);
}
