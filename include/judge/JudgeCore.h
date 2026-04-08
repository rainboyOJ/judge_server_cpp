#pragma once

#include <vector>

#include "common/SubmissionTypes.h"

// JudgeCore 只关心测试点结果如何汇总，不关心语言执行细节。
class JudgeCore {
  public:
    SubmissionVerdict summarize(
        const std::vector<SubmissionCaseResult> &case_results,
        SubmissionVerdict default_verdict = SubmissionVerdict::PENDING) const;
};
