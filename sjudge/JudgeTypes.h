#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief 资源限制的“无限制”哨兵值。
 *
 * sjudger 内部约定所有资源限制字段为 0 时表示不启用该限制。
 * 这个常量主要用于提高调用方组装 `judge_config` 时的可读性。
 */
constexpr uint64_t SJUDGE_UNLIMITED = 0;

/**
 * @brief 一次评测执行所需的完整配置。
 *
 * runner 层把题目测试点、编译产物路径、资源限制等信息转换成这个结构，
 * 然后交给 `run_sjudger()`。这个结构只描述“怎么运行程序”，不包含
 * 输出比较、题目语义或提交状态机。
 */
struct judge_config {
  /** @brief CPU 时间限制，单位毫秒；0 表示不限制。 */
  uint32_t max_cpu_time_ms{0};
  /** @brief 真实时间限制，单位毫秒；0 表示不限制。 */
  uint32_t max_real_time_ms{0};
  /** @brief 地址空间限制，单位字节；0 表示不限制。 */
  uint64_t max_memory_bytes{0};
  /** @brief 栈大小限制，单位字节；0 表示不限制。 */
  uint64_t max_stack_bytes{0};
  /** @brief 输出文件大小限制，单位字节；0 表示不限制。 */
  uint64_t max_output_bytes{0};
  /** @brief 子进程工作目录；为空时不切换目录。 */
  std::string cwd;
  /** @brief 要执行的程序路径；不能为空。 */
  std::string exe_path{"1"};
  /** @brief 标准输入来源文件路径。 */
  std::string input_path{"/dev/stdin"};
  /** @brief 标准输出目标文件路径。 */
  std::string output_path{"/dev/stdout"};
  /** @brief 标准错误目标文件路径。 */
  std::string error_path{"/dev/stderr"};
  /** @brief 兼容旧外部 sjudge 的日志路径；内置 sjudger 当前不写这个文件。 */
  std::string log_path{"judger_log.txt"};
  /** @brief 传给 execve 的 argv；为空时使用 exe_path 作为 argv[0]。 */
  std::vector<std::string> args;
  /** @brief 预留环境变量字段；当前实现仍沿用父进程 environ。 */
  std::vector<std::string> env;
};

/**
 * @brief sjudger 结果码和基础设施错误码。
 *
 * 历史接口把 verdict 和 error code 放在同一个 enum 中。为了兼容现有
 * 调用方暂时保留这个布局，但使用时要区分：
 * - `judge_result::result` 使用前 6 个 verdict 类值。
 * - `judge_result::error` 使用 WAIT_FAILED 之后的基础设施错误值。
 */
enum judgeResult_id {
  /// 程序成功运行结束；runner 后续还要比较输出，才能得到最终 AC/WA。
  SUCCESS = 0,
  /// CPU 时间超限。
  CPU_TIME_LIMIT_EXCEEDED,
  /// 真实时间超限。
  REAL_TIME_LIMIT_EXCEEDED,
  /// 内存超限。
  MEMORY_LIMIT_EXCEEDED,
  /// 用户程序运行时错误，如非零退出码或异常信号。
  RUNTIME_ERROR,
  /// judger 基础设施错误。
  SYSTEM_ERROR,

  /// wait4/waitpid 失败。
  WAIT_FAILED,
  /// 配置非法。
  INVALID_CONFIG,
  /// fork 失败。
  FORK_FAILED,
  /// 历史保留：线程创建失败。
  PTHREAD_FAILED,
  /// 历史保留：需要 root 权限。
  ROOT_REQUIRED,
  /// seccomp 策略加载失败。
  LOAD_SECCOMP_FAILED,
  /// setrlimit 失败。
  SETRLIMIT_FAILED,
  /// open/dup2 重定向标准流失败。
  DUP2_FAILED,
  /// 历史保留：setuid 失败。
  SETUID_FAILED,
  /// execve 失败。
  EXECVE_FAILED,

  /// 历史保留：special judge 错误。
  SPJ_ERROR,
  /// 历史保留：编译失败。
  COMPILE_FAIL
};

/**
 * @brief 一次程序执行后的底层结果。
 *
 * 这个结构只表达 sjudger 观察到的执行状态。输出是否正确由 runner 在
 * `SUCCESS` 后继续比较用户输出和标准输出。
 */
struct judge_result {
  /** @brief 用户态 CPU 时间，单位毫秒。 */
  int cpu_time_ms{0};
  /** @brief 真实运行时间，单位毫秒。 */
  int real_time_ms{0};
  /** @brief 内存峰值，单位字节。 */
  long long memory_bytes{0};
  /** @brief 被信号终止时的信号值；正常退出为 0。 */
  int signal{0};
  /** @brief 正常退出时的 exit code；被信号终止时通常为 0。 */
  int exit_code{0};
  /** @brief 基础设施错误码；无错误为 0。 */
  int error{0};
  /** @brief verdict 类结果码，默认 SYSTEM_ERROR。 */
  int result{SYSTEM_ERROR};
};
