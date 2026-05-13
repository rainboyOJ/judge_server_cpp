#pragma once

#include <vector>

#include "common/SubmissionTypes.h"

/**
 * @brief 负责把多个测试点结果汇总成最终 verdict。
 *
 * 它不关心代码如何执行，只关心已有 case 结果的归并规则。
 */
class SubmissionVerdictReducer {
  public:
    /**
     * @brief 汇总多个测试点结果。
     * @param case_results 当前 submission 已得到的测试点结果列表。
     * @param default_verdict 当输入为空时返回的默认 verdict。
     * @return SubmissionVerdict 汇总后的最终结论。
     */
    SubmissionVerdict summarize(
        const std::vector<SubmissionCaseResult> &case_results,
        SubmissionVerdict default_verdict = SubmissionVerdict::PENDING) const;
};
