/**
 * @file RunnerCompileSupport.cpp
 * @brief 旧 test_process 流程中的编译能力实现，同时对新 runner 提供兼容 helper。
 */

#include "common/Logger.h"
#include "runner/RunnerSupport.h"
#include "common/utils.h"
#include <filesystem>

namespace fs = std::filesystem;

/** @copydoc compile_cpp_source_file */
bool compile_cpp_source_file(const fs::path &source_file,
                             const fs::path &executable_file,
                             const fs::path &compile_log_file,
                             std::string &compile_output) {
    const std::string compile_command =
        "g++ -std=c++17 -O2 -DONLINE_JUDGE  -o " + executable_file.string() +
        " " + source_file.string() + " 2> " + compile_log_file.string();
    try {
        compile_output = run_command_capture_stdout(compile_command.c_str());
        return fs::exists(executable_file);
    } catch (const std::exception &) {
        compile_output.clear();
        return false;
    }
}
