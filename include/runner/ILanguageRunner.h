#pragma once

#include <string>

#include "common/SubmissionTypes.h"

struct RunnerPrepareResult {
    bool ok{false};
    std::string message;
};

struct RunnerCompileResult {
    bool ok{false};
    SubmissionVerdict verdict{SubmissionVerdict::PENDING};
    std::string message;
};

struct RunnerCaseInput {
    int seq_id{};
    std::string input_path;
    std::string expected_output_path;
    int cpu_time_limit_ms{};
    int real_time_limit_ms{};
    unsigned long long memory_limit_kb{};
};

struct RunnerCaseResult {
    SubmissionCaseResult result;
    std::string message;
};

class ILanguageRunner {
  public:
    virtual ~ILanguageRunner() = default;

    // 中文注释：runner 只负责语言相关动作，状态流转与结果汇总仍由上层服务控制。
    virtual RunnerPrepareResult prepare(const SubmissionRequest &request) = 0;
    virtual RunnerCompileResult compile(const SubmissionRequest &request) = 0;
    virtual RunnerCaseResult runCase(const SubmissionRequest &request,
                                     const RunnerCaseInput &test_case) = 0;
};
