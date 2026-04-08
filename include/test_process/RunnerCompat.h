#pragma once

#include <filesystem>
#include <string>

#include "common/SubmissionTypes.h"
#include "runner/ILanguageRunner.h"

namespace fs = std::filesystem;

// 从旧评测流程中抽出的最小复用接口，供新 runner 逐步接轨。
bool compile_cpp_source_file(const fs::path &source_file,
                             const fs::path &executable_file,
                             const fs::path &compile_log_file,
                             std::string &compile_output);

SubmissionVerdict compare_case_output(const fs::path &input_path,
                                      const fs::path &expected_output,
                                      const fs::path &user_output);

RunnerCaseResult run_executable_case(const fs::path &executable_path,
                                     const RunnerCaseInput &test_case,
                                     const fs::path &user_output_path);
