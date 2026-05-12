#include <fcntl.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

extern char **environ;
namespace fs = std::filesystem;

enum class Verdict {
    AC,
    TLE,
    MLE,
    RE,
    SYSTEM_ERROR,
};

struct Config {
    std::string exe;
    std::vector<std::string> args;
    std::string input;
    std::string output;
    int real_time_limit_ms{1000};
    int cpu_time_limit_ms{1000};
    unsigned long long memory_limit_bytes{0};
};

struct Result {
    Verdict verdict{Verdict::SYSTEM_ERROR};
    int exit_code{0};
    int signal{0};
    int real_time_ms{0};
    int cpu_time_ms{0};
    long long memory_bytes{0};
};

namespace {

int allocate_memory() {
    std::vector<std::string> blocks;
    while (true) {
        blocks.emplace_back(8 * 1024 * 1024, 'x');
    }
}

const char *to_string(Verdict verdict) {
    switch (verdict) {
    case Verdict::AC:
        return "AC";
    case Verdict::TLE:
        return "TLE";
    case Verdict::MLE:
        return "MLE";
    case Verdict::RE:
        return "RE";
    case Verdict::SYSTEM_ERROR:
        return "SYSTEM_ERROR";
    }
    return "UNKNOWN";
}

int timeval_to_ms(const timeval &value) {
    return static_cast<int>(value.tv_sec * 1000 + value.tv_usec / 1000);
}

void redirect_or_exit(const std::string &path, int flags, int target_fd) {
    const int fd = open(path.c_str(), flags, 0644);
    if (fd < 0 || dup2(fd, target_fd) < 0) {
        _exit(100);
    }
    close(fd);
}

void apply_memory_limit_or_exit(unsigned long long bytes) {
    if (bytes == 0) {
        return;
    }
    rlimit limit{};
    limit.rlim_cur = static_cast<rlim_t>(bytes);
    limit.rlim_max = static_cast<rlim_t>(bytes);
    if (setrlimit(RLIMIT_AS, &limit) != 0) {
        _exit(101);
    }
}

Verdict map_result(const Config &config, const Result &raw, bool timed_out) {
    if (timed_out) {
        return Verdict::TLE;
    }
    if (config.memory_limit_bytes > 0 &&
        raw.memory_bytes > static_cast<long long>(config.memory_limit_bytes)) {
        return Verdict::MLE;
    }
    if (config.memory_limit_bytes > 0 &&
        (raw.signal == SIGSEGV || raw.signal == SIGABRT ||
         raw.signal == SIGKILL || raw.exit_code == 127)) {
        return Verdict::MLE;
    }
    if (config.cpu_time_limit_ms > 0 && raw.cpu_time_ms > config.cpu_time_limit_ms) {
        return Verdict::TLE;
    }
    if (raw.signal != 0 || raw.exit_code != 0) {
        return Verdict::RE;
    }
    return Verdict::AC;
}

Result run_case(const Config &config) {
    Result result{};
    const pid_t pid = fork();
    if (pid == 0) {
        redirect_or_exit(config.input, O_RDONLY, STDIN_FILENO);
        redirect_or_exit(config.output, O_WRONLY | O_CREAT | O_TRUNC, STDOUT_FILENO);
        apply_memory_limit_or_exit(config.memory_limit_bytes);

        std::vector<char *> argv;
        for (const std::string &arg : config.args) {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        if (argv.empty()) {
            argv.push_back(const_cast<char *>(config.exe.c_str()));
        }
        argv.push_back(nullptr);

        execve(config.exe.c_str(), argv.data(), environ);
        _exit(127);
    }

    if (pid < 0) {
        result.verdict = Verdict::SYSTEM_ERROR;
        return result;
    }

    const auto start = std::chrono::steady_clock::now();
    bool timed_out = false;
    while (true) {
        int status = 0;
        rusage usage{};
        const pid_t rc = wait4(pid, &status, WNOHANG, &usage);
        if (rc == pid) {
            result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
            result.signal = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
            result.cpu_time_ms = timeval_to_ms(usage.ru_utime);
            result.memory_bytes = static_cast<long long>(usage.ru_maxrss) * 1024;
            result.real_time_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count());
            break;
        }
        if (rc < 0) {
            result.verdict = Verdict::SYSTEM_ERROR;
            return result;
        }

        result.real_time_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start)
                .count());
        if (config.real_time_limit_ms > 0 &&
            result.real_time_ms > config.real_time_limit_ms) {
            timed_out = true;
            kill(pid, SIGKILL);
            status = 0;
            usage = {};
            wait4(pid, &status, 0, &usage);
            result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
            result.signal = WIFSIGNALED(status) ? WTERMSIG(status) : SIGKILL;
            result.cpu_time_ms = timeval_to_ms(usage.ru_utime);
            result.memory_bytes = static_cast<long long>(usage.ru_maxrss) * 1024;
            break;
        }
        usleep(1000);
    }

    result.verdict = map_result(config, result, timed_out);
    return result;
}

Config make_config(const std::string &mode, const std::string &self_path) {
    fs::create_directories("demo_tmp");
    {
        std::ofstream input("demo_tmp/mini.in");
        input << "hello\n";
    }

    Config config{};
    config.input = "demo_tmp/mini.in";
    config.output = "demo_tmp/mini.out";

    if (mode == "ok") {
        config.exe = "/bin/cat";
        config.args = {"/bin/cat"};
    } else if (mode == "re") {
        config.exe = "/bin/sh";
        config.args = {"/bin/sh", "-c", "exit 3"};
    } else if (mode == "tle") {
        config.exe = "/bin/sleep";
        config.args = {"/bin/sleep", "10"};
        config.real_time_limit_ms = 300;
    } else if (mode == "mle") {
        config.exe = self_path;
        config.args = {
            self_path,
            "--alloc-memory",
        };
        config.memory_limit_bytes = 64ULL * 1024 * 1024;
    } else {
        config.exe = "/bin/sh";
        config.args = {"/bin/sh", "-c", "echo unknown mode; exit 2"};
    }

    return config;
}

} // namespace

int main(int argc, char **argv) {
    if (argc > 1 && std::strcmp(argv[1], "--alloc-memory") == 0) {
        return allocate_memory();
    }

    const std::string mode = argc > 1 ? argv[1] : "ok";
    const Config config = make_config(mode, argv[0]);
    const Result result = run_case(config);

    std::cout << "verdict=" << to_string(result.verdict)
              << " exit_code=" << result.exit_code
              << " signal=" << result.signal
              << " real_time_ms=" << result.real_time_ms
              << " cpu_time_ms=" << result.cpu_time_ms
              << " memory_bytes=" << result.memory_bytes << "\n";
    return 0;
}
