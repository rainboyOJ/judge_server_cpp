#include "ChildSetup.h"

#include <fcntl.h>
#include <sys/resource.h>
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

void apply_limit(int resource, uint64_t value) {
    if (value == 0) {
        return;
    }

    rlimit limit{};
    limit.rlim_cur = static_cast<rlim_t>(value);
    limit.rlim_max = static_cast<rlim_t>(value);
    if (setrlimit(resource, &limit) != 0) {
        _exit(SETRLIMIT_FAILED);
    }
}

} // namespace

[[noreturn]] void run_child_process_or_exit(const judge_config &config) {
    if (!config.cwd.empty() && chdir(config.cwd.c_str()) != 0) {
        _exit(SYSTEM_ERROR);
    }

    redirect_fd(config.input_path, O_RDONLY, STDIN_FILENO);
    redirect_fd(config.output_path, O_WRONLY | O_CREAT | O_TRUNC, STDOUT_FILENO);
    redirect_fd(config.error_path, O_WRONLY | O_CREAT | O_TRUNC, STDERR_FILENO);

    apply_limit(RLIMIT_STACK, config.max_stack_bytes);
    apply_limit(RLIMIT_AS, config.max_memory_bytes);
    apply_limit(RLIMIT_FSIZE, config.max_output_bytes);
    if (config.max_cpu_time_ms > 0) {
        const uint64_t cpu_seconds = (config.max_cpu_time_ms + 999) / 1000;
        apply_limit(RLIMIT_CPU, cpu_seconds);
    }

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
