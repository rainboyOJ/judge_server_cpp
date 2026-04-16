#pragma once

#include <string>
#include <vector>

/**
 * @brief 表示一份 submission 在评测生命周期中的当前阶段。
 *
 * 这些状态由 @ref SubmissionService 和 @ref ResultStore 共同维护，
 * 用来描述“一份提交现在走到哪一步了”。
 */
enum class SubmissionStatus {
    QUEUED,
    PREPARING,
    COMPILING,
    RUNNING,
    FINISHED,
    FAILED,
};

/**
 * @brief 当前系统支持或预留的提交语言类型。
 */
enum class SubmissionLanguage {
    CPP = 0,
    C = 1,
    PYTHON = 2,
};

/**
 * @brief 单个测试点或整份 submission 的判题结论。
 */
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

/**
 * @brief 协议层解码后的统一提交请求对象。
 *
 * 这个结构表示“用户提交了什么”，会从 TCP/JSON 请求中解析得到，
 * 然后在调度层、服务层和执行层之间传递。
 */
struct SubmissionRequest {
    int uuid{};
    std::string pid;
    SubmissionLanguage language{SubmissionLanguage::CPP};
    std::string code;
};

/**
 * @brief 单个测试点的标准化执行结果。
 *
 * 这是 runner 层对一个 case 执行后的统一表达，
 * judge 层会基于多个 @ref SubmissionCaseResult 汇总最终 verdict。
 */
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

/**
 * @brief 一份 submission 在任意时刻的完整结果快照。
 *
 * @note 这是异步系统中的核心状态对象：
 * - 入队后可表示 QUEUED
 * - 编译中可表示 COMPILING
 * - 运行中可携带部分 case_results
 * - 完成后表示最终 verdict
 */
struct SubmissionResult {
    int submission_id{-1};
    SubmissionStatus status{SubmissionStatus::QUEUED};
    SubmissionVerdict verdict{SubmissionVerdict::PENDING};
    std::string message;
    std::vector<SubmissionCaseResult> case_results;
};
