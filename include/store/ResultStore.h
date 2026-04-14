#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "common/SubmissionTypes.h"

class ResultStore {
public:
  int createSubmission(const SubmissionRequest &request);
  bool updateResult(int submission_id, const SubmissionResult &result);
  bool getResult(int submission_id, SubmissionResult &result) const;
  void debugForceAgingForTest(int submission_id, int age_seconds);
  void debugConfigureCleanupForTest(int retention_seconds,
                                    int max_stored_results);

private:
  using Clock = std::chrono::steady_clock;

  struct StoredSubmission {
    SubmissionResult result;
    Clock::time_point last_access_at;
    std::optional<Clock::time_point> finished_at;
  };

  static bool isValidTransition(SubmissionStatus current,
                                SubmissionStatus next);
  void cleanupExpiredResultsLocked(Clock::time_point now);
  int resultRetentionSecondsLocked() const;
  int maxStoredResultsLocked() const;

  // 中文注释：状态机只允许按评测流程向前推进；终态写入后不再接受回退。
  mutable std::mutex mutex_;
  int next_submission_id_{1};
  std::unordered_map<int, StoredSubmission> results_;
  std::optional<int> override_retention_seconds_;
  std::optional<int> override_max_stored_results_;
};
