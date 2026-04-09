#include "dispatch/JudgeWorkerPool.h"

#include "service/SubmissionService.h"

JudgeWorkerPool::JudgeWorkerPool(std::size_t worker_count,
                                 SubmissionQueue &queue,
                                 SubmissionService &service,
                                 SubmissionNotifier *notifier)
    : queue_(queue), service_(service), notifier_(notifier) {
  workers_.reserve(worker_count);
  for (std::size_t i = 0; i < worker_count; ++i) {
    workers_.emplace_back(&JudgeWorkerPool::workerLoop, this);
  }
}

JudgeWorkerPool::~JudgeWorkerPool() { stop(); }

void JudgeWorkerPool::stop() {
  bool expected = false;
  if (!stopped_.compare_exchange_strong(expected, true)) {
    return;
  }

  queue_.shutdown();
  for (std::thread &worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

void JudgeWorkerPool::workerLoop() {
  SubmissionTask task{};
  while (queue_.pop(task)) {
    if (notifier_ != nullptr) {
      notifier_->onSubmissionStarted(task);
    }

    service_.processSubmission(task.submission_id, task.request);

    if (notifier_ != nullptr) {
      notifier_->onSubmissionFinished(task);
    }
  }
}
