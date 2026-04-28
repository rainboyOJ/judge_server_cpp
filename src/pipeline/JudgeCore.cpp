/**
 * @file JudgeCore.cpp
 * @brief 最终 verdict 汇总规则实现。
 */

#include "pipeline/JudgeCore.h"

namespace {

/**
 * @brief 为每种 verdict 分配比较用的优先级。
 */
int verdict_priority(SubmissionVerdict verdict) {
    switch (verdict) {
    case SubmissionVerdict::CE:
        return 100;
    case SubmissionVerdict::SYSTEM_ERROR:
        return 90;
    case SubmissionVerdict::TLE:
        return 80;
    case SubmissionVerdict::RE:
    case SubmissionVerdict::MLE:
    case SubmissionVerdict::OLE:
        return 70;
    case SubmissionVerdict::WA:
    case SubmissionVerdict::PE:
        return 60;
    case SubmissionVerdict::UNKNOWN:
        return 50;
    case SubmissionVerdict::AC:
        return 10;
    case SubmissionVerdict::PENDING:
    default:
        return 0;
    }
}

} // namespace

/** @copydoc JudgeCore::summarize */
SubmissionVerdict
JudgeCore::summarize(const std::vector<SubmissionCaseResult> &case_results,
                     SubmissionVerdict default_verdict) const {
    // 中文注释：空输入直接返回调用方给出的默认值，避免在还没有任何测试点结果时
    // 人为制造一个“伪最终结论”。
    if (case_results.empty()) {
        return default_verdict;
    }

    // 中文注释：MVP 只保留一套简单且确定的优先级。
    // 规则从高到低为：CE > SYSTEM_ERROR > TLE > RE/MLE/OLE > WA/PE > UNKNOWN >
    // AC > PENDING。 这样可以明确表达：
    // 1. 编译失败直接盖过运行期结果；
    // 2. TLE 比普通运行错误更优先；
    // 3. RE 会压过 WA/AC，但不会压过 CE/TLE；
    // 4. UNKNOWN 与 SYSTEM_ERROR 也有稳定归并结果，不依赖输入顺序。
    SubmissionVerdict summary = case_results.front().verdict;
    int summary_priority = verdict_priority(summary);

    for (const SubmissionCaseResult &case_result : case_results) {
        const int current_priority = verdict_priority(case_result.verdict);
        if (current_priority > summary_priority) {
            summary = case_result.verdict;
            summary_priority = current_priority;
        }
    }

    return summary;
}
