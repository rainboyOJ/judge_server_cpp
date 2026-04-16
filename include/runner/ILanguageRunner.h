#pragma once

#include <string>

#include "common/SubmissionTypes.h"

/**
 * @brief runner 预处理阶段的返回结果。
 */
struct RunnerPrepareResult {
    bool ok{false};
    std::string message;
};

/**
 * @brief runner 编译/语法检查阶段的返回结果。
 */
struct RunnerCompileResult {
    bool ok{false};
    SubmissionVerdict verdict{SubmissionVerdict::PENDING};
    std::string message;
};

/**
 * @brief runner 执行单个测试点时所需的输入描述。
 */
struct RunnerCaseInput {
    int seq_id{};
    std::string input_path;
    std::string expected_output_path;
    int cpu_time_limit_ms{};
    int real_time_limit_ms{};
    unsigned long long memory_limit_kb{};
};

/**
 * @brief runner 执行单个测试点后的标准化结果。
 */
struct RunnerCaseResult {
    SubmissionCaseResult result;
    std::string message;
};

/**
 * @brief 所有语言 runner 的统一接口。
 */
class ILanguageRunner {
  public:
    virtual ~ILanguageRunner() = default;

    /**
     * @brief 执行语言相关的预处理动作。
     */
    virtual RunnerPrepareResult prepare(const SubmissionRequest &request) = 0;
    /**
     * @brief 执行编译或语法检查。
     */
    virtual RunnerCompileResult compile(const SubmissionRequest &request) = 0;
    /**
     * @brief 执行单个测试点。
     */
    virtual RunnerCaseResult runCase(const SubmissionRequest &request,
                                     const RunnerCaseInput &test_case) = 0;
};
