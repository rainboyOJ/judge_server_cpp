
#ifndef SJUDGE_CALL_H
#define SJUDGE_CALL_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// 定义常量
constexpr uint64_t SJUDGE_UNLIMITED = 0;

// 评测参数结构体
struct judge_config {
    uint32_t max_cpu_time_ms{0};
    uint32_t max_real_time_ms{0};
    uint64_t max_memory_bytes{0};
    uint64_t max_stack_bytes{0};
    uint64_t max_output_bytes{0};
    std::string cwd;                        // current work directory
    std::string exe_path{"1"};              // executable path
    std::string input_path{"/dev/stdin"};   // input file path
    std::string output_path{"/dev/stdout"}; // output file path
    std::string error_path{"/dev/stderr"};  // error file path
    std::string log_path{"judger_log.txt"}; // log file path
    std::vector<std::string> args;
    std::vector<std::string> env;
};

// 评测结果ID枚举
enum judgeResult_id {
    SUCCESS = 0,
    CPU_TIME_LIMIT_EXCEEDED,
    REAL_TIME_LIMIT_EXCEEDED,
    MEMORY_LIMIT_EXCEEDED,
    RUNTIME_ERROR,
    SYSTEM_ERROR,
    WAIT_FAILED,
    INVALID_CONFIG,
    FORK_FAILED,
    PTHREAD_FAILED,
    ROOT_REQUIRED,
    LOAD_SECCOMP_FAILED,
    SETRLIMIT_FAILED,
    DUP2_FAILED,
    SETUID_FAILED,
    EXECVE_FAILED,
    SPJ_ERROR,
    COMPILE_FAIL
};

// 评测结果结构体
struct judge_result {
    int cpu_time_ms{0};
    int real_time_ms{0};
    long long memory_bytes{0};
    int signal{0};
    int exit_code{0};
    int error{0};
    int result{SYSTEM_ERROR};
};

// 调用 sjudge 二进制文件进行评测
judge_result call_sjudge(const char *sjudge_binary_path,
                         const judge_config &config);

judge_result run_sjudger(const judge_config &config);

#endif // SJUDGE_CALL_H
