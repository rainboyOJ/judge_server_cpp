#include "judgeInfo.h"
#include "workThreadPool.h"
#include "common/Logger.h"
#include "utils.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// 编译阶段
bool Compile(const int testBoxId, resultContainer *resultContainerPtr,workThreadPool * workThreadPoolPtr) {
    LOG_DEBUG("Compile testBoxId %d", testBoxId);
    language lang = resultContainerPtr->getLanguage(testBoxId);
    if (lang == language::cpp ){
        LOG_DEBUG("Compile cpp");

        // 得到代码
        std::string_view code = resultContainerPtr->getCode(testBoxId);
        LOG_DEBUG("Code: %s", code.data());
        
        // 创建临时目录用于编译
        fs::path tempDir = resultContainerPtr->getWorkDir(testBoxId) ;
        LOG_DEBUG("Temp dir: %s", tempDir.c_str());
        std::error_code ec;
        if( !fs::exists(tempDir))
            fs::create_directory(tempDir,ec);
        else
            fs::remove_all(tempDir,ec);
        
        // 将代码写入临时文件
        fs::path sourceFile = tempDir / "solution.cpp";
        std::ofstream sourceStream(sourceFile);
        if (!sourceStream) {
            LOG_ERROR("Failed to create source file: %s", sourceFile.c_str());
            resultContainerPtr->setErrorType(testBoxId, testError::compile_error);
            return false;
        }
        sourceStream << code;
        sourceStream.close();
        
        // 构建编译命令
        auto exeName = resultContainerPtr->getExeName(testBoxId);
        fs::path executableFile = tempDir / exeName;
        std::string compileCmd = "g++ -std=c++17 -O2 -DONLINE_JUDGE  -o " + executableFile.string() + " " + sourceFile.string() + " 2>&1";
        LOG_DEBUG("Compile command: %s", compileCmd.c_str());
        
        // 执行编译命令
        try {
            std::string compileOutput = Popen(compileCmd.c_str());
            
            // 检查编译是否成功
            if (fs::exists(executableFile)) {
                LOG_DEBUG("Compile success for testBoxId %d", testBoxId);
                // 编译成功，可以保存可执行文件路径到resultContainer或其他地方
                // TODO: 保存可执行文件路径供后续测试使用
                return true;
            } else {
                LOG_DEBUG("Compile failed for testBoxId %d. Output: %s", testBoxId, compileOutput.c_str());
                // 编译失败，设置错误类型
                resultContainerPtr->setErrorType(testBoxId, testError::compile_error);
                return false;
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Compile exception for testBoxId %d: %s", testBoxId, e.what());
            resultContainerPtr->setErrorType(testBoxId, testError::compile_error);
            return false;
        }
    }
    else { // 
        
        // 向resultContainer 写入错误信息: do not support language
        LOG_ERROR("Unsupported language for testBoxId %d", testBoxId);
        resultContainerPtr->setErrorType(testBoxId, testError::compile_error);
        return false;
    }
    return true;
}