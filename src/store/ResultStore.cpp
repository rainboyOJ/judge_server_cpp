#include "store/ResultStore.h"

#include <algorithm>
#include <utility>

#include "common/Config.h"

namespace {

bool is_terminal_status(SubmissionStatus status) {
  return status == SubmissionStatus::FINISHED ||
         status == SubmissionStatus::FAILED;
}

} // namespace

bool ResultStore::isValidTransition(SubmissionStatus current,
                                    SubmissionStatus next) {
  if (is_terminal_status(current)) {
    return false;
  }

  if (current == next) {
    return true;
  }

  // 中文注释：这里只接受最小可读状态机，既避免回退，也避免跳过关键阶段。
  switch (current) {
  case SubmissionStatus::QUEUED:
    return next == SubmissionStatus::PREPARING ||
           next == SubmissionStatus::FAILED;
  case SubmissionStatus::PREPARING:
    return next == SubmissionStatus::COMPILING ||
           next == SubmissionStatus::FAILED;
  case SubmissionStatus::COMPILING:
    return next == SubmissionStatus::RUNNING ||
           next == SubmissionStatus::FINISHED ||
           next == SubmissionStatus::FAILED;
  case SubmissionStatus::RUNNING:
    return next == SubmissionStatus::FINISHED ||
           next == SubmissionStatus::FAILED;
  case SubmissionStatus::FINISHED:
  case SubmissionStatus::FAILED:
    return false;
  }

  return false;
}

int ResultStore::resultRetentionSecondsLocked() const {
  if (override_retention_seconds_.has_value()) {
    return *override_retention_seconds_;
  }
  return Config::getInstance().getResultRetentionSeconds();
}

int ResultStore::maxStoredResultsLocked() const {
  if (override_max_stored_results_.has_value()) {
    return *override_max_stored_results_;
  }
  return Config::getInstance().getMaxStoredResults();
}

void ResultStore::cleanupExpiredResultsLocked(Clock::time_point now) {
  const int retention_seconds = resultRetentionSecondsLocked();
  const int max_stored_results = maxStoredResultsLocked();

  if (retention_seconds >= 0) {
    const auto retention = std::chrono::seconds(retention_seconds);
    for (auto it = results_.begin(); it != results_.end();) {
      if (!it->second.finished_at.has_value()) {
        ++it;
        continue;
      }

      if (now - *it->second.finished_at >= retention) {
        it = results_.erase(it);
      } else {
        ++it;
      }
    }
  }

  if (max_stored_results <= 0 ||
      static_cast<int>(results_.size()) <= max_stored_results) {
    return;
  }

  std::vector<std::pair<int, Clock::time_point>> finished_entries;
  finished_entries.reserve(results_.size());
  for (const auto &[submission_id, stored] : results_) {
    if (stored.finished_at.has_value()) {
      finished_entries.push_back({submission_id, *stored.finished_at});
    }
  }

  std::sort(finished_entries.begin(), finished_entries.end(),
            [](const auto &lhs, const auto &rhs) {
              return lhs.second < rhs.second;
            });

  for (const auto &[submission_id, _] : finished_entries) {
    if (static_cast<int>(results_.size()) <= max_stored_results) {
      break;
    }
    results_.erase(submission_id);
  }
}

int ResultStore::createSubmission(const SubmissionRequest &request) {
  (void)request;

  std::lock_guard<std::mutex> lock(mutex_);
  cleanupExpiredResultsLocked(Clock::now());
  const int submission_id = next_submission_id_++;

  StoredSubmission stored{};
  stored.result.submission_id = submission_id;
  stored.result.status = SubmissionStatus::QUEUED;
  stored.last_access_at = Clock::now();
  results_[submission_id] = std::move(stored);
  return submission_id;
}

bool ResultStore::updateResult(int submission_id,
                               const SubmissionResult &result) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now = Clock::now();
  cleanupExpiredResultsLocked(now);
  const auto it = results_.find(submission_id);
  if (it == results_.end()) {
    return false;
  }

  if (!isValidTransition(it->second.result.status, result.status)) {
    return false;
  }

  SubmissionResult next_result = result;
  next_result.submission_id = submission_id;
  it->second.result = std::move(next_result);
  it->second.last_access_at = now;
  if (is_terminal_status(it->second.result.status)) {
    it->second.finished_at = now;
  }

  cleanupExpiredResultsLocked(now);
  return true;
}

bool ResultStore::getResult(int submission_id, SubmissionResult &result) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now = Clock::now();
  const_cast<ResultStore *>(this)->cleanupExpiredResultsLocked(now);
  const auto it = results_.find(submission_id);
  if (it == results_.end()) {
    return false;
  }

  const_cast<StoredSubmission &>(it->second).last_access_at = now;
  result = it->second.result;
  return true;
}

void ResultStore::debugForceAgingForTest(int submission_id, int age_seconds) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = results_.find(submission_id);
  if (it == results_.end()) {
    return;
  }

  const auto aged_time = Clock::now() - std::chrono::seconds(age_seconds);
  it->second.last_access_at = aged_time;
  if (is_terminal_status(it->second.result.status)) {
    it->second.finished_at = aged_time;
  }
}

void ResultStore::debugConfigureCleanupForTest(int retention_seconds,
                                               int max_stored_results) {
  std::lock_guard<std::mutex> lock(mutex_);
  override_retention_seconds_ = retention_seconds;
  override_max_stored_results_ = max_stored_results;
}
