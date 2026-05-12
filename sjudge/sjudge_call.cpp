#include "ChildSetup.h"
#include "ConfigValidator.h"
#include "ParentMonitor.h"
#include "ResultMapper.h"
#include "sjudge_call.h"

#ifdef SJUDGER_ENABLE_SECCOMP
#include "SeccompPolicy.h"
#endif

#include <array>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <unistd.h>

namespace {

bool is_child_setup_error_code(int exit_code) {
    switch (exit_code) {
    case SYSTEM_ERROR:
    case WAIT_FAILED:
    case INVALID_CONFIG:
    case FORK_FAILED:
    case LOAD_SECCOMP_FAILED:
    case SETRLIMIT_FAILED:
    case DUP2_FAILED:
    case SETUID_FAILED:
    case EXECVE_FAILED:
        return true;
    default:
        return false;
    }
}

} // namespace

judge_result call_sjudge(const char *sjudge_binary_path,
                         const judge_config &config) {
    std::string command = sjudge_binary_path;

    if (config.max_cpu_time_ms != SJUDGE_UNLIMITED) {
        command += " --max-cpu-time " + std::to_string(config.max_cpu_time_ms);
    }

    if (config.max_real_time_ms != SJUDGE_UNLIMITED) {
        command += " --max-real-time " +
                   std::to_string(config.max_real_time_ms);
    }

    if (config.max_memory_bytes != SJUDGE_UNLIMITED) {
        command += " --max-memory " + std::to_string(config.max_memory_bytes);
    }

    if (config.max_stack_bytes != SJUDGE_UNLIMITED) {
        command += " --max-stack " + std::to_string(config.max_stack_bytes);
    }

    if (config.max_output_bytes != SJUDGE_UNLIMITED) {
        command += " --max-output " + std::to_string(config.max_output_bytes);
    }

    if (!config.cwd.empty()) {
        command += " --cwd " + config.cwd;
    }

    if (!config.exe_path.empty()) {
        command += " --exe " + config.exe_path;
    }

    if (!config.input_path.empty()) {
        command += " --input " + config.input_path;
    }

    if (!config.output_path.empty()) {
        command += " --output " + config.output_path;
    }

    if (!config.error_path.empty()) {
        command += " --error " + config.error_path;
    }

    if (!config.log_path.empty()) {
        command += " --log " + config.log_path;
    }

    std::array<char, 128> buffer;
    std::string output;

    command += " 2>&1";

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"),
                                                  pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        output += buffer.data();
    }

    judge_result jresult;
    std::istringstream iss(output);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.find("cpu_time") == 0) {
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                jresult.cpu_time_ms = std::stoi(line.substr(pos));
            }
        } else if (line.find("real_time") == 0) {
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                jresult.real_time_ms = std::stoi(line.substr(pos));
            }
        } else if (line.find("memory") == 0) {
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                jresult.memory_bytes = std::stoll(line.substr(pos));
            }
        } else if (line.find("signal") == 0) {
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                jresult.signal = std::stoi(line.substr(pos));
            }
        } else if (line.find("exit_code") == 0) {
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                jresult.exit_code = std::stoi(line.substr(pos));
            }
        } else if (line.find("error") == 0) {
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                jresult.error = std::stoi(line.substr(pos));
            }
        } else if (line.find("result") == 0) {
            size_t pos = line.find_first_of("0123456789");
            if (pos != std::string::npos) {
                jresult.result = std::stoi(line.substr(pos));
            }
        }
    }

    return jresult;
}

judge_result run_sjudger(const judge_config &config) {
    judge_result result{};
    int error_code = 0;
    if (!validate_judge_config(config, error_code)) {
        result.result = SYSTEM_ERROR;
        result.error = error_code;
        return result;
    }

#ifdef SJUDGER_ENABLE_SECCOMP
    if (!apply_seccomp_if_enabled(config, result.error)) {
        return result;
    }
#endif

    const pid_t pid = fork();
    if (pid == 0) {
        run_child_process_or_exit(config);
    }
    if (pid < 0) {
        result.result = SYSTEM_ERROR;
        result.error = FORK_FAILED;
        return result;
    }

    const monitor_result monitor =
        monitor_child_process(pid, config.max_real_time_ms);
    result.cpu_time_ms = monitor.cpu_time_ms;
    result.real_time_ms = monitor.real_time_ms;
    result.memory_bytes = monitor.memory_bytes;
    result.signal = monitor.signal;
    result.exit_code = monitor.exit_code;

    if (monitor.error != 0) {
        result.result = SYSTEM_ERROR;
        result.error = monitor.error;
        return result;
    }

    if (result.signal == 0 && result.exit_code != 0 &&
        is_child_setup_error_code(result.exit_code)) {
        result.result = SYSTEM_ERROR;
        result.error = result.exit_code;
        return result;
    }

    result.error = 0;
    result.result = map_monitor_result_to_judge_code(config, monitor);
    return result;
}
