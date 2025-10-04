#include "workThreadPool.h"
#include "common/Logger.h"
#include "sjudge_call.h"
#include "judgeInfo.h"
#include "resultContainer.h"
#include "testBox.h"
#include "utils.h"
#include <filesystem>

namespace fs = std::filesystem;

bool TestOneSinglePoint(const int testBoxId, int seq_id, resultContainer *resultContainerPtr) {
    LOG_DEBUG("TestOneSinglePoint Start, testBoxId %d, seq_id %d", testBoxId, seq_id);
    
    // 获取题目ID
    char pid[32];
    strcpy(pid, resultContainerPtr->getPid(testBoxId));
    
    auto testBoxPtr = resultContainerPtr->getTestBoxPtr();
    auto problemPath = testBoxPtr->getProbelmPath();

    // 获取可执行文件路径（假设在预处理阶段已编译好）
    fs::path tempDir = resultContainerPtr->work_dir(testBoxId);
    fs::path sourceFile = resultContainerPtr->source_path(testBoxId);
    fs::path executableFile = resultContainerPtr->exe_path(testBoxId);

    if (!fs::exists(executableFile)) {
        LOG_ERROR("Executable file not found for testBoxId %d", testBoxId);
        return false;
    }
    
    // 获取测试用例信息
    TestCaseInfo testCaseInfo;
    if (!resultContainerPtr->getTestCaseInfo(testBoxId, seq_id, testCaseInfo)) {
        LOG_ERROR("Failed to get test case info for testBoxId %d, seq_id %d", testBoxId, seq_id);
        return false;
    }
    
    // 构建评测配置
    judge_config config;
    config.max_cpu_time = testCaseInfo.cpu_time_limit;
    config.max_real_time = testCaseInfo.real_time_limit;
    config.max_memory = testCaseInfo.memory_limit;
    config.exe_path = executableFile.string();
    config.input_path = testCaseInfo.input_path;
    config.output_path = testCaseInfo.user_output_path;
    config.cwd = testCaseInfo.cwd;
    
    // 调用sjudge进行评测
    try {
        judge_result result = call_sjudge("/usr/bin/sjudge", config);
        
        // 处理评测结果
        LOG_DEBUG("Test result - cpu_time: %d, real_time: %d, memory: %ld, result: %d", 
                  result.cpu_time, result.real_time, result.memory, result.result);
        
        // 将结果写入resultContainer
        TestCaseResult testCaseResult;
        testCaseResult.testBoxId = testBoxId;
        testCaseResult.seq_id = seq_id;
        testCaseResult.cpu_time = result.cpu_time;
        testCaseResult.real_time = result.real_time;
        testCaseResult.memory = result.memory;
        testCaseResult.signal = result.signal;
        testCaseResult.exit_code = result.exit_code;
        testCaseResult.error = result.error;
        
        // 根据sjudge的结果映射到enum_testStatus
        switch (result.result) {
            case 0: // SUCCESS
                testCaseResult.result = enum_testStatus::AC;
                break;
            case 1: // CPU_TIME_LIMIT_EXCEEDED
                testCaseResult.result = enum_testStatus::TLE;
                break;
            case 2: // REAL_TIME_LIMIT_EXCEEDED
                testCaseResult.result = enum_testStatus::TLE;
                break;
            case 3: // MEMORY_LIMIT_EXCEEDED
                testCaseResult.result = enum_testStatus::MLE;
                break;
            case 4: // RUNTIME_ERROR
                testCaseResult.result = enum_testStatus::RE;
                break;
            default:
                testCaseResult.result = enum_testStatus::UNKNOWN;
                break;
        }
        
        // 写入结果到resultContainer
        // TODO : 完善 ,比如是否 超时, 输入输出是否一致等
        // 通过testBoxPtr来写入结果
        resultContainerPtr->writeCaseResult(testBoxId, seq_id, testCaseResult);
        
        // 根据评测结果判断是否成功
        return (result.result == 0); // 假设0表示成功
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during test execution for testBoxId %d, seq_id %d: %s", testBoxId, seq_id, e.what());
        return false;
    }
    
    return true;
}