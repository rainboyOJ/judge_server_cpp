/**
 * @file Compile.cpp
 * @brief 旧 test_process 流程中的编译能力实现，同时对新 runner 提供兼容 helper。
 */

#include "common/Logger.h"
#include "legacy/judgeInfo.h"
#include "test_process/RunnerCompat.h"
#include "utils.h"
#include "legacy/workThreadPool.h"
#include <filesystem>
#include <fstream>
#include <sstream>

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

/**
 * @brief 旧链路中的编译阶段入口。
 *
 * @note 这是历史兼容流程的一部分，当前异步主链路优先使用 CppRunner。
 */
bool Compile(const int testBoxId, resultContainer *resultContainerPtr,
             workThreadPool *workThreadPoolPtr) {
    LOG_DEBUG("Compile testBoxId %d", testBoxId);
    language lang = resultContainerPtr->getLanguage(testBoxId);
    if (lang == language::cpp) {
        LOG_DEBUG("Compile cpp");

        // 得到代码
        std::string_view code = resultContainerPtr->getCode(testBoxId);
        LOG_DEBUG("Code: %s", code.data());

        // 创建临时目录用于编译
        fs::path tempDir = resultContainerPtr->work_dir(testBoxId);
        fs::path sourceFile = resultContainerPtr->source_path(testBoxId);
        fs::path executableFile = resultContainerPtr->exe_path(testBoxId);

        std::error_code ec;
        if (!fs::exists(tempDir))
            fs::create_directory(tempDir, ec);
        else
            fs::remove_all(tempDir, ec);

        // 将代码写入临时文件
        std::ofstream sourceStream(sourceFile);
        if (!sourceStream) {
            LOG_ERROR("Failed to create source file: %s", sourceFile.c_str());
            resultContainerPtr->setErrorType(testBoxId,
                                             testError::compile_error);
            return false;
        }
        sourceStream << code;
        sourceStream.close();

        try {
            std::string compileOutput;

            // 检查编译是否成功
            if (compile_cpp_source_file(sourceFile, executableFile,
                                        tempDir / "compile.log",
                                        compileOutput)) {
                LOG_DEBUG("Compile success for testBoxId %d", testBoxId);
                // 编译成功，可以保存可执行文件路径到resultContainer或其他地方
                // TODO: 保存可执行文件路径供后续测试使用
                return true;
            } else {
                LOG_DEBUG("Compile failed for testBoxId %d. Output: %s",
                          testBoxId, compileOutput.c_str());
                // 编译失败，设置错误类型
                resultContainerPtr->setErrorType(testBoxId,
                                                 testError::compile_error);
                return false;
            }
        } catch (const std::exception &e) {
            LOG_ERROR("Compile exception for testBoxId %d: %s", testBoxId,
                      e.what());
            resultContainerPtr->setErrorType(testBoxId,
                                             testError::compile_error);
            return false;
        }
    } else { //

        // 向resultContainer 写入错误信息: do not support language
        LOG_ERROR("Unsupported language for testBoxId %d", testBoxId);
        resultContainerPtr->setErrorType(testBoxId, testError::compile_error);
        return false;
    }
    return true;
}
