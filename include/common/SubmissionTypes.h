#pragma once

#include <string>
#include <vector>

// 统一的提交状态，供协议层、服务层、存储层共享。
enum class SubmissionStatus {
    QUEUED,
    PREPARING,
    COMPILING,
    RUNNING,
    FINISHED,
    FAILED,
};

// 统一的语言枚举，避免跨模块直接依赖旧结构体中的裸 int。
enum class SubmissionLanguage {
    CPP = 0,
    C = 1,
    PYTHON = 2,
};

// 统一的判题结果枚举，供中间结果和最终结果复用。
enum class SubmissionVerdict {
    PENDING = -1,
    AC = 0,
    WA = 1,
    TLE = 2,
    MLE = 3,
    RE = 4,
    OLE = 5,
    PE = 6,
    CE = 7,
    UNKNOWN = 8,
    SYSTEM_ERROR = 9,
};

// 提交请求的最小公共表示。
struct SubmissionRequest {
    int uuid{};
    std::string pid;
    SubmissionLanguage language{SubmissionLanguage::CPP};
    std::string code;
};

// 单个测试点的标准化结果。
struct SubmissionCaseResult {
    int seq_id{};
    SubmissionVerdict verdict{SubmissionVerdict::PENDING};
    int cpu_time_ms{};
    int real_time_ms{};
    unsigned long long memory_kb{};
    int signal{};
    int exit_code{};
    int error_code{};
};

// 提交在任意时刻的结果快照。
struct SubmissionResult {
    int submission_id{-1};
    SubmissionStatus status{SubmissionStatus::QUEUED};
    SubmissionVerdict verdict{SubmissionVerdict::PENDING};
    std::string message;
    std::vector<SubmissionCaseResult> case_results;
};
