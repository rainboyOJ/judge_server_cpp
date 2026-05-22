/**
 * @file RunnerExecutionSupport.cpp
 * @brief 旧链路单点执行与新 runner 共用执行 helper 的实现。
 */

#include "common/Config.h"
#include "common/Logger.h"
#include "runner/RunnerSupport.h"
#include "sjudge_call.h"
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace {

/** @brief 去掉一行右侧多余空白字符。 */
std::string trim_line_right(const std::string &line) {
  size_t end = line.size();
  while (end > 0 && (line[end - 1] == ' ' || line[end - 1] == '\t' ||
                     line[end - 1] == '\r')) {
    --end;
  }
  return line.substr(0, end);
}

/** @brief 归一化输出文件为按行比较的形式。 */
std::vector<std::string> normalize_output_lines(const fs::path &path) {
  std::ifstream stream(path);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(trim_line_right(line));
  }
  while (!lines.empty() && lines.back().empty()) {
    lines.pop_back();
  }
  return lines;
}

/** @brief 把 sjudge 的 result code 映射成内部 verdict。 */
SubmissionVerdict map_judge_result_to_verdict(int result_code) {
  switch (result_code) {
  case 0:
    return SubmissionVerdict::AC;
  case 1:
  case 2:
    return SubmissionVerdict::TLE;
  case 3:
    return SubmissionVerdict::MLE;
  case 4:
    return SubmissionVerdict::RE;
  case 5:
    return SubmissionVerdict::SYSTEM_ERROR;
  default:
    return SubmissionVerdict::UNKNOWN;
  }
}

} // namespace

/** @copydoc runner_work_dir_for */
fs::path runner_work_dir_for(const SubmissionRequest &request) {
  return fs::temp_directory_path() /
         ("oj_compile_" + std::to_string(request.uuid));
}

/** @copydoc compare_case_output */
SubmissionVerdict compare_case_output(const fs::path &input_path,
                                      const fs::path &expected_output,
                                      const fs::path &user_output) {
  const fs::path checkerPath = "/judge/checker/fcmp2";
  if (fs::exists(checkerPath)) {
    const std::string checkerCmd =
        checkerPath.string() + " " + input_path.string() + " " +
        user_output.string() + " " + expected_output.string();
    return system(checkerCmd.c_str()) == 0 ? SubmissionVerdict::AC
                                           : SubmissionVerdict::WA;
  }

  return normalize_output_lines(expected_output) ==
                 normalize_output_lines(user_output)
             ? SubmissionVerdict::AC
             : SubmissionVerdict::WA;
}

/** @copydoc run_executable_case */
RunnerCaseResult run_executable_case(const fs::path &executable_path,
                                     const RunnerCaseInput &test_case,
                                     const fs::path &user_output_path) {
  RunnerCaseResult case_result{};
  case_result.result.seq_id = test_case.seq_id;

  if (!fs::exists(executable_path) || !fs::exists(test_case.input_path) ||
      !fs::exists(test_case.expected_output_path)) {
    case_result.result.verdict = SubmissionVerdict::SYSTEM_ERROR;
    case_result.message = "missing executable or case files";
    return case_result;
  }

  judge_config config;
  config.max_cpu_time_ms =
      test_case.cpu_time_limit_ms > 0
          ? static_cast<uint32_t>(test_case.cpu_time_limit_ms)
          : SJUDGE_UNLIMITED;
  config.max_real_time_ms =
      test_case.real_time_limit_ms > 0
          ? static_cast<uint32_t>(test_case.real_time_limit_ms)
          : SJUDGE_UNLIMITED;
  config.max_memory_bytes =
      test_case.memory_limit_kb > 0
          ? static_cast<uint64_t>(test_case.memory_limit_kb) * 1024
          : SJUDGE_UNLIMITED;
  config.cwd = executable_path.parent_path().string();
  config.exe_path = executable_path.string();
  config.args = {executable_path.string()};
  config.input_path = test_case.input_path;
  config.output_path = user_output_path.string();
  config.error_path = user_output_path.string() + ".err";

  const Config &server_config = Config::getInstance();
  // 这些字段只影响 fork 出来的用户程序子进程；judge_server 父进程保持原权限。
  config.run_uid = server_config.getSjudgeRunUid();
  config.run_gid = server_config.getSjudgeRunGid();
  config.clear_supplementary_groups =
      server_config.getSjudgeClearSupplementaryGroups();
  config.enable_seccomp = server_config.getSjudgeEnableSeccomp();
  config.seccomp_deny_network = server_config.getSjudgeSeccompDenyNetwork();
  config.seccomp_deny_process_spawn =
      server_config.getSjudgeSeccompDenyProcessSpawn();
  config.cgroup_path = server_config.getSjudgeCgroupPath();
  config.cgroup_memory_max_bytes =
      server_config.getSjudgeCgroupMemoryMaxBytes();
  config.cgroup_pids_max = server_config.getSjudgeCgroupPidsMax();
  config.cgroup_cpu_max_quota_us = server_config.getSjudgeCgroupCpuMaxQuotaUs();
  config.cgroup_cpu_max_period_us =
      server_config.getSjudgeCgroupCpuMaxPeriodUs();

  const judge_result result = run_sjudger(config);
  case_result.result.cpu_time_ms = result.cpu_time_ms;
  case_result.result.real_time_ms = result.real_time_ms;
  case_result.result.memory_kb =
      result.memory_bytes > 0
          ? static_cast<unsigned long long>(result.memory_bytes / 1024)
          : 0;
  case_result.result.signal = result.signal;
  case_result.result.exit_code = result.exit_code;
  case_result.result.error_code = result.error;
  case_result.result.verdict = map_judge_result_to_verdict(result.result);

  if (result.result == SYSTEM_ERROR) {
    case_result.message = "failed to execute in-process sjudger";
    return case_result;
  }

  if (case_result.result.verdict == SubmissionVerdict::AC) {
    case_result.result.verdict = compare_case_output(
        test_case.input_path, test_case.expected_output_path, user_output_path);
    case_result.message = case_result.result.verdict == SubmissionVerdict::AC
                              ? user_output_path.string()
                              : "output mismatch";
  }

  return case_result;
}
