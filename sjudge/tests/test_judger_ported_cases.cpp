#include "sjudge_call.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;

namespace {

fs::path make_workspace(const std::string &name) {
  const auto suffix =
      std::to_string(getpid()) + "_" +
      std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count());
  const fs::path dir = fs::temp_directory_path() / (name + "_" + suffix);
  std::error_code ec;
  fs::remove_all(dir, ec);
  assert(fs::create_directories(dir, ec));
  assert(!ec);
  return dir;
}

std::string read_file(const fs::path &path) {
  std::ifstream stream(path);
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

void write_file(const fs::path &path, const std::string &content) {
  std::ofstream stream(path);
  stream << content;
}

bool run_command(const std::string &command) {
  return std::system(command.c_str()) == 0;
}

fs::path source_root() {
  return fs::path(PROJECT_ROOT_DIR) / "sjudge" / "tests" / "test_src";
}

fs::path compile_c(const fs::path &workspace, const fs::path &source) {
  const fs::path exe = workspace / source.stem();
  const std::string command =
      "gcc -g -O0 " + source.string() + " -o " + exe.string();
  assert(run_command(command));
  return exe;
}

fs::path compile_cpp(const fs::path &workspace, const fs::path &source) {
  const fs::path exe = workspace / source.stem();
  const std::string command =
      "g++ -g -O0 " + source.string() + " -o " + exe.string();
  assert(run_command(command));
  return exe;
}

judge_config base_config(const fs::path &workspace, const fs::path &exe) {
  judge_config config{};
  config.exe_path = exe.string();
  config.args = {exe.string()};
  config.cwd = workspace.string();
  config.input_path = "/dev/null";
  config.output_path = (workspace / "stdout.txt").string();
  config.error_path = (workspace / "stderr.txt").string();
  config.max_cpu_time_ms = 1000;
  config.max_real_time_ms = 3000;
  config.max_memory_bytes = 128ULL * 1024 * 1024;
  config.max_stack_bytes = 32ULL * 1024 * 1024;
  config.max_output_bytes = 1024ULL * 1024;
  return config;
}

judge_result run_case(const judge_config &config) {
  const judge_result result = run_sjudger(config);
  assert(result.error == 0);
  return result;
}

void test_normal_program_and_math_output() {
  const fs::path workspace = make_workspace("sjudge_ported_normal");
  const fs::path normal =
      compile_c(workspace, source_root() / "integration" / "normal.c");
  const fs::path input = workspace / "input.txt";
  const fs::path output = workspace / "output.txt";
  write_file(input, "judger_test\n");

  judge_config config = base_config(workspace, normal);
  config.input_path = input.string();
  config.output_path = output.string();
  config.error_path = (workspace / "error.txt").string();

  judge_result result = run_case(config);
  assert(result.result == SUCCESS);
  assert(read_file(output) == "judger_test\nHello world");

  const fs::path math =
      compile_c(workspace, source_root() / "integration" / "math.c");
  config = base_config(workspace, math);
  config.output_path = output.string();
  result = run_case(config);
  assert(result.result == SUCCESS);
  assert(read_file(output) == "abs 1024");
}

void test_argv_is_passed_to_execve() {
  const fs::path workspace = make_workspace("sjudge_ported_args");
  const fs::path exe =
      compile_c(workspace, source_root() / "integration" / "args.c");
  const fs::path output = workspace / "args.out";

  judge_config config = base_config(workspace, exe);
  config.args = {exe.string(), "test", "hehe", "000"};
  config.output_path = output.string();

  const judge_result result = run_case(config);
  assert(result.result == SUCCESS);

  const std::string expected = "argv[0]: " + exe.string() +
                               "\nargv[1]: test\nargv[2]: hehe\nargv[3]: "
                               "000\n";
  assert(read_file(output) == expected);
}

void test_stdout_and_stderr_can_share_output_file() {
  const fs::path workspace = make_workspace("sjudge_ported_stdio");
  const fs::path exe =
      compile_c(workspace, source_root() / "integration" / "stdout_stderr.c");
  const fs::path output = workspace / "combined.out";

  judge_config config = base_config(workspace, exe);
  config.output_path = output.string();
  config.error_path = output.string();

  const judge_result result = run_case(config);
  assert(result.result == SUCCESS);
  assert(read_file(output).find("stdout\n") != std::string::npos);
  assert(read_file(output).find("stderr\n") != std::string::npos);
}

void test_real_time_limit_kills_sleeping_program() {
  const fs::path workspace = make_workspace("sjudge_ported_real_time");
  const fs::path exe =
      compile_c(workspace, source_root() / "integration" / "sleep.c");

  judge_config config = base_config(workspace, exe);
  config.max_real_time_ms = 50;

  const judge_result result = run_case(config);
  assert(result.result == REAL_TIME_LIMIT_EXCEEDED);
  assert(result.signal != 0);
  assert(result.real_time_ms >= 50);
}

void test_cpu_time_limit_detects_busy_loop() {
  const fs::path workspace = make_workspace("sjudge_ported_cpu_time");
  const fs::path exe =
      compile_c(workspace, source_root() / "integration" / "while1.c");

  judge_config config = base_config(workspace, exe);
  config.max_cpu_time_ms = 20;
  config.max_real_time_ms = 2000;

  const judge_result result = run_case(config);
  assert(result.result == CPU_TIME_LIMIT_EXCEEDED ||
         result.result == REAL_TIME_LIMIT_EXCEEDED);
  assert(result.signal != 0);
}

void test_memory_limit_cases() {
  const fs::path workspace = make_workspace("sjudge_ported_memory");
  fs::path exe =
      compile_c(workspace, source_root() / "integration" / "memory1.c");

  judge_config config = base_config(workspace, exe);
  config.max_memory_bytes = 64ULL * 1024 * 1024;
  judge_result result = run_case(config);
  assert(result.result == MEMORY_LIMIT_EXCEEDED);

  exe = compile_c(workspace, source_root() / "integration" / "memory2.c");
  config = base_config(workspace, exe);
  config.max_memory_bytes = 64ULL * 1024 * 1024;
  result = run_case(config);
  assert(result.result == RUNTIME_ERROR ||
         result.result == MEMORY_LIMIT_EXCEEDED);

  exe = compile_c(workspace, source_root() / "integration" / "memory3.c");
  config = base_config(workspace, exe);
  config.max_memory_bytes = 512ULL * 1024 * 1024;
  result = run_case(config);
  assert(result.result == SUCCESS);
  assert(result.memory_bytes >= 300LL * 1024 * 1024);
}

void test_runtime_error_exit_code_and_signal() {
  const fs::path workspace = make_workspace("sjudge_ported_runtime_error");
  fs::path exe = compile_c(workspace, source_root() / "integration" / "re1.c");

  judge_config config = base_config(workspace, exe);
  judge_result result = run_case(config);
  assert(result.result == RUNTIME_ERROR);
  assert(result.exit_code == 25);

  exe = compile_c(workspace, source_root() / "integration" / "re2.c");
  config = base_config(workspace, exe);
  config.max_memory_bytes = 0;
  result = run_case(config);
  assert(result.result == RUNTIME_ERROR);
  assert(result.signal != 0);
}

void test_output_size_limit_uses_rlimit_fsize() {
  const fs::path workspace = make_workspace("sjudge_ported_output_size");
  const fs::path exe =
      compile_c(workspace, source_root() / "integration" / "output_size.c");
  const fs::path output_file = workspace / "fsize_test";

  judge_config config = base_config(workspace, exe);
  config.max_output_bytes = 10 * 1024;

  const judge_result result = run_case(config);
  assert(result.result == RUNTIME_ERROR);
  assert(fs::exists(output_file));
  assert(fs::file_size(output_file) <= config.max_output_bytes);
}

void test_stack_limit_can_reject_large_stack_allocation() {
  const fs::path workspace = make_workspace("sjudge_ported_stack");
  const fs::path exe =
      compile_c(workspace, source_root() / "integration" / "stack.c");

  judge_config config = base_config(workspace, exe);
  config.max_memory_bytes = 256ULL * 1024 * 1024;
  config.max_stack_bytes = 16ULL * 1024 * 1024;
  judge_result result = run_case(config);
  assert(result.result == RUNTIME_ERROR ||
         result.result == MEMORY_LIMIT_EXCEEDED);

  config.max_stack_bytes = 128ULL * 1024 * 1024;
  result = run_case(config);
  assert(result.result == SUCCESS);
}

void test_cpp_writev_style_io() {
  const fs::path workspace = make_workspace("sjudge_ported_writev");
  const fs::path exe =
      compile_cpp(workspace, source_root() / "integration" / "writev.cpp");
  const fs::path input = workspace / "input.txt";
  const fs::path output = workspace / "output.txt";
  const std::string text(30000, '1');
  write_file(input, text + "\n");

  judge_config config = base_config(workspace, exe);
  config.input_path = input.string();
  config.output_path = output.string();

  const judge_result result = run_case(config);
  assert(result.result == SUCCESS);
  assert(read_file(output) == text);
}

void test_uid_gid_defaults_match_parent_when_not_configured() {
  const fs::path workspace = make_workspace("sjudge_ported_uid_gid");
  const fs::path exe =
      compile_c(workspace, source_root() / "integration" / "uid_gid.c");
  const fs::path output = workspace / "uid_gid.out";

  judge_config config = base_config(workspace, exe);
  config.output_path = output.string();
  config.error_path = output.string();
  config.enable_seccomp = true;
  config.seccomp_deny_process_spawn = false;

  const judge_result result = run_case(config);
  assert(result.result == SUCCESS);
  assert(read_file(output).find("uid " + std::to_string(getuid())) !=
         std::string::npos);
  assert(read_file(output).find("gid " + std::to_string(getgid())) !=
         std::string::npos);
}

#ifdef SJUDGER_ENABLE_SECCOMP
void test_seccomp_blocks_fork_when_enabled() {
  const fs::path workspace = make_workspace("sjudge_ported_seccomp_fork");
  const fs::path exe =
      compile_c(workspace, source_root() / "seccomp" / "fork.c");

  judge_config config = base_config(workspace, exe);
  config.enable_seccomp = false;
  judge_result result = run_case(config);
  assert(result.result == SUCCESS);

  config.enable_seccomp = true;
  config.seccomp_deny_process_spawn = true;
  result = run_case(config);
  assert(result.result == RUNTIME_ERROR);
  assert(result.signal != 0);
}

void test_execve_remains_allowed_for_current_default_allow_seccomp_policy() {
  const fs::path workspace = make_workspace("sjudge_ported_seccomp_execve");
  const fs::path exe =
      compile_c(workspace, source_root() / "seccomp" / "execve.c");
  const fs::path output = workspace / "execve.out";

  judge_config config = base_config(workspace, exe);
  config.output_path = output.string();
  config.enable_seccomp = false;
  judge_result result = run_case(config);
  assert(result.result == SUCCESS);
  assert(read_file(output) == "Helloworld\n");

  config.enable_seccomp = true;
  config.seccomp_deny_process_spawn = true;
  result = run_case(config);
  assert(result.result == SUCCESS);
  assert(read_file(output) == "Helloworld\n");
}
#endif

} // namespace

int main() {
  test_normal_program_and_math_output();
  test_argv_is_passed_to_execve();
  test_stdout_and_stderr_can_share_output_file();
  test_real_time_limit_kills_sleeping_program();
  test_cpu_time_limit_detects_busy_loop();
  test_memory_limit_cases();
  test_runtime_error_exit_code_and_signal();
  test_output_size_limit_uses_rlimit_fsize();
  test_stack_limit_can_reject_large_stack_allocation();
  test_cpp_writev_style_io();
  test_uid_gid_defaults_match_parent_when_not_configured();
#ifdef SJUDGER_ENABLE_SECCOMP
  test_seccomp_blocks_fork_when_enabled();
  test_execve_remains_allowed_for_current_default_allow_seccomp_policy();
#endif
  return 0;
}
