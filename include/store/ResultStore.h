#pragma once

#include <mutex>
#include <unordered_map>

#include "common/SubmissionTypes.h"

class ResultStore {
  public:
    int createSubmission(const SubmissionRequest &request);
    bool updateResult(int submission_id, const SubmissionResult &result);
    bool getResult(int submission_id, SubmissionResult &result) const;

  private:
    static bool isValidTransition(SubmissionStatus current,
                                  SubmissionStatus next);

    // 中文注释：状态机只允许按评测流程向前推进；终态写入后不再接受回退。
    mutable std::mutex mutex_;
    int next_submission_id_{1};
    std::unordered_map<int, SubmissionResult> results_;
};
