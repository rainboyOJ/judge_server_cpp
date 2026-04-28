#pragma once

#include <filesystem>
#include <string>

#include "common/SubmissionTypes.h"
#include "runner/ILanguageRunner.h"

namespace fs = std::filesystem;

/**
 * @file RunnerCompat.h
 * @brief 从旧 test_process 流程中抽出的最小复用接口。
 *
 * 这些函数的作用不是保留旧架构主导权，而是把已经验证过的“编译/执行/输出比较”
 * 能力复用给新的 runner 层，避免重复造轮子。
 */

/**
 * @brief 编译一份 C++ 源文件。
 * @param source_file 输入源文件路径。
 * @param executable_file 目标可执行文件路径。
 * @param compile_log_file 编译日志文件路径。
 * @param compile_output 输出参数，返回编译器文本输出。
 * @return true 编译成功且目标可执行文件存在。
 * @return false 编译失败。
 */
bool compile_cpp_source_file(const fs::path &source_file,
                             const fs::path &executable_file,
                             const fs::path &compile_log_file,
                             std::string &compile_output);

/**
 * @brief 比较用户输出与标准输出是否一致。
 *
 * 若系统中存在 checker，则优先调用 checker；否则退化为本地文本比较。
 */
SubmissionVerdict compare_case_output(const fs::path &input_path,
                                      const fs::path &expected_output,
                                      const fs::path &user_output);

/**
 * @brief 执行一个可执行目标并返回标准化的单点测试结果。
 *
 * 这个函数是 C++ / Python runner 共用的执行入口：
 * - 若存在 sjudge，则优先交给 sjudge 执行
 * - 否则使用本地 fallback 执行路径
 */
RunnerCaseResult run_executable_case(const fs::path &executable_path,
                                     const RunnerCaseInput &test_case,
                                     const fs::path &user_output_path);
