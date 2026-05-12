#pragma once

#include <cstdint>
#include <string>
#include <vector>

constexpr uint64_t SJUDGE_UNLIMITED = 0;

struct judge_config {
  uint32_t max_cpu_time_ms{0};
  uint32_t max_real_time_ms{0};
  uint64_t max_memory_bytes{0};
  uint64_t max_stack_bytes{0};
  uint64_t max_output_bytes{0};
  std::string cwd;
  std::string exe_path{"1"};
  std::string input_path{"/dev/stdin"};
  std::string output_path{"/dev/stdout"};
  std::string error_path{"/dev/stderr"};
  std::string log_path{"judger_log.txt"};
  std::vector<std::string> args;
  std::vector<std::string> env;
};

enum judgeResult_id {
  // Verdict codes returned in judge_result::result.
  SUCCESS = 0,
  CPU_TIME_LIMIT_EXCEEDED,
  REAL_TIME_LIMIT_EXCEEDED,
  MEMORY_LIMIT_EXCEEDED,
  RUNTIME_ERROR,
  SYSTEM_ERROR,

  // Infrastructure error codes returned in judge_result::error.
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

  // Legacy/special-purpose codes retained for compatibility.
  SPJ_ERROR,
  COMPILE_FAIL
};

struct judge_result {
  int cpu_time_ms{0};
  int real_time_ms{0};
  long long memory_bytes{0};
  int signal{0};
  int exit_code{0};
  int error{0};
  int result{SYSTEM_ERROR};
};
