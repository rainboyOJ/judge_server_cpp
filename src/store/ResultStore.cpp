#include "store/ResultStore.h"

#include <utility>

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

int ResultStore::createSubmission(const SubmissionRequest &request) {
    (void)request;

    std::lock_guard<std::mutex> lock(mutex_);
    const int submission_id = next_submission_id_++;

    SubmissionResult initial_result{};
    initial_result.submission_id = submission_id;
    initial_result.status = SubmissionStatus::QUEUED;
    results_[submission_id] = initial_result;
    return submission_id;
}

bool ResultStore::updateResult(int submission_id,
                               const SubmissionResult &result) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = results_.find(submission_id);
    if (it == results_.end()) {
        return false;
    }

    if (!isValidTransition(it->second.status, result.status)) {
        return false;
    }

    SubmissionResult next_result = result;
    next_result.submission_id = submission_id;
    it->second = std::move(next_result);
    return true;
}

bool ResultStore::getResult(int submission_id, SubmissionResult &result) const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = results_.find(submission_id);
    if (it == results_.end()) {
        return false;
    }

    result = it->second;
    return true;
}
