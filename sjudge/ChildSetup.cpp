#include "ChildSetup.h"

#include "SeccompPolicy.h"

#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#include <string>
#include <vector>

extern char **environ;

namespace {

/**
 * @brief 切换到用户程序运行目录。
 *
 * `cwd` 只是工作目录，不是文件系统隔离。切换失败说明评测环境配置有误，
 * 子进程用 SYSTEM_ERROR 退出，让父进程识别为基础设施错误。
 */
void change_working_directory_or_exit(const judge_config &config) {
  if (!config.cwd.empty() && chdir(config.cwd.c_str()) != 0) {
    _exit(SYSTEM_ERROR);
  }
}

/**
 * @brief 把指定文件重定向到标准 fd。
 *
 * 用户程序只需要读 stdin、写 stdout/stderr；具体文件路径由 judger 在
 * execve 前接好。open 或 dup2 失败不是用户程序 RE，而是 DUP2_FAILED。
 */
void redirect_fd_or_exit(const std::string &path, int open_flags,
                         int target_fd) {
  const int fd = open(path.c_str(), open_flags, 0644);
  if (fd < 0 || dup2(fd, target_fd) < 0) {
    _exit(DUP2_FAILED);
  }
  close(fd);
}

/**
 * @brief 重定向 stdin/stdout/stderr 三个标准流。
 *
 * 这一步必须在 seccomp 前完成，否则真实 seccomp 策略可能禁止 open/dup2，
 * 导致子进程连执行环境都准备不了。
 */
void redirect_standard_streams_or_exit(const judge_config &config) {
  redirect_fd_or_exit(config.input_path, O_RDONLY, STDIN_FILENO);
  redirect_fd_or_exit(config.output_path, O_WRONLY | O_CREAT | O_TRUNC,
                      STDOUT_FILENO);
  redirect_fd_or_exit(config.error_path, O_WRONLY | O_CREAT | O_TRUNC,
                      STDERR_FILENO);
}

/**
 * @brief 应用单项 rlimit。
 *
 * 0 表示不限制。限制在子进程设置，随后被 execve 后的用户程序继承，
 * 不会影响父进程继续 wait4/kill/记录结果。
 */
void apply_limit_or_exit(int resource, uint64_t value) {
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

/**
 * @brief 应用 sjudger 当前支持的资源限制。
 *
 * `RLIMIT_CPU` 只能按秒设置，所以毫秒限制向上取整；毫秒级判定由
 * wait4 采样后的 ResultMapper 继续兜底。
 */
void apply_resource_limits_or_exit(const judge_config &config) {
  apply_limit_or_exit(RLIMIT_STACK, config.max_stack_bytes);
  apply_limit_or_exit(RLIMIT_AS, config.max_memory_bytes);
  apply_limit_or_exit(RLIMIT_FSIZE, config.max_output_bytes);
  if (config.max_cpu_time_ms > 0) {
    const uint64_t cpu_seconds = (config.max_cpu_time_ms + 999) / 1000;
    apply_limit_or_exit(RLIMIT_CPU, cpu_seconds);
  }
}

/**
 * @brief 构造 execve 需要的 argv 数组。
 *
 * execve 要求 `char *const argv[]`，这里的 char* 指向 config.args 内部
 * 字符串；在 execve 调用前 config 仍然有效，所以这个转换是安全的。
 */
std::vector<char *> build_argv(const judge_config &config) {
  std::vector<char *> argv;
  for (const std::string &arg : config.args) {
    argv.push_back(const_cast<char *>(arg.c_str()));
  }
  if (argv.empty()) {
    argv.push_back(const_cast<char *>(config.exe_path.c_str()));
  }
  argv.push_back(nullptr);
  return argv;
}

/**
 * @brief 加载 seccomp 策略。
 *
 * 当前实现是 stub。保留这个步骤是为了让真实 seccomp 接入时流程位置明确：
 * 资源和 IO 已准备好，下一步就是 execve 用户程序。
 */
void apply_seccomp_policy_or_exit(const judge_config &config) {
  int error_code = 0;
  if (!apply_seccomp_if_enabled(config, error_code)) {
    _exit(error_code);
  }
}

/**
 * @brief 执行用户程序。
 *
 * execve 成功后不会返回；如果返回，说明执行失败，需要用明确错误码退出，
 * 让父进程把它映射成 SYSTEM_ERROR。
 */
[[noreturn]] void exec_program_or_exit(const judge_config &config) {
  std::vector<char *> argv = build_argv(config);

  execve(config.exe_path.c_str(), argv.data(), environ);
  _exit(EXECVE_FAILED);
}

} // namespace

[[noreturn]] void run_child_process_or_exit(const judge_config &config) {
  // 子进程只做“准备环境 -> execve”这一条直线流程，避免复杂逻辑污染 fork
  // 后状态。
  change_working_directory_or_exit(config);
  redirect_standard_streams_or_exit(config);
  apply_resource_limits_or_exit(config);
  apply_seccomp_policy_or_exit(config);
  exec_program_or_exit(config);
}
