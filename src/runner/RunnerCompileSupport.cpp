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
bool compile_cpp_source_file(const fs::path &sourceFile,
                             const fs::path &executableFile,
                             const fs::path &compileLogFile,
                             std::string &compileOutput) {
    const std::string compileCmd =
        "g++ -std=c++17 -O2 -DONLINE_JUDGE  -o " + executableFile.string() +
        " " + sourceFile.string() + " 2> " + compileLogFile.string();
    try {
        compileOutput = Popen(compileCmd.c_str());
        return fs::exists(executableFile);
    } catch (const std::exception &) {
        compileOutput.clear();
        return false;
    }
}
