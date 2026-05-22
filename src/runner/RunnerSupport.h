#pragma once

#include <filesystem>
#include <string>

#include "common/SubmissionTypes.h"
#include "runner/ILanguageRunner.h"

namespace fs = std::filesystem;

/**
 * @file RunnerSupport.h
 * @brief runner 主链路复用的编译、执行与输出比较辅助接口。
 *
 * 这些函数属于当前运行时路径的一部分：
 * - `CppRunner` 复用编译与执行辅助能力
 * - `PythonRunner` 复用执行与输出比较辅助能力
 *
 * 它不是 legacy 兼容层，而是 runner 模块的共享支撑入口。
 */

/** @brief 编译一份 C++ 源文件。 */
bool compile_cpp_source_file(const fs::path &source_file,
                             const fs::path &executable_file,
                             const fs::path &compile_log_file,
                             std::string &compile_output);

/** @brief 计算某次 submission 的 runner 工作目录。 */
fs::path runner_work_dir_for(const SubmissionRequest &request);

/** @brief 比较用户输出与标准输出。 */
SubmissionVerdict compare_case_output(const fs::path &input_path,
                                      const fs::path &expected_output,
                                      const fs::path &user_output);

/** @brief 执行一个可执行目标并返回标准化单点结果。 */
RunnerCaseResult run_executable_case(const fs::path &executable_path,
                                     const RunnerCaseInput &test_case,
                                     const fs::path &user_output_path);
