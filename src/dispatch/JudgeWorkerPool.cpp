/**
 * @file JudgeWorkerPool.cpp
 * @brief 后台异步评测 worker 线程池实现。
 */

#include "dispatch/JudgeWorkerPool.h"

#include <sstream>

#include "common/Logger.h"
#include "pipeline/SubmissionService.h"

/** @copydoc JudgeWorkerPool::JudgeWorkerPool */
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

/** @copydoc JudgeWorkerPool::~JudgeWorkerPool */
JudgeWorkerPool::~JudgeWorkerPool() { stop(); }

/** @copydoc JudgeWorkerPool::stop */
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

/** @copydoc JudgeWorkerPool::graceful_shutdown */
void JudgeWorkerPool::graceful_shutdown() {
  stopped_.store(true);
  stop();
}

/** @copydoc JudgeWorkerPool::workerLoop */
void JudgeWorkerPool::workerLoop() {
  std::ostringstream worker_id_stream;
  worker_id_stream << std::this_thread::get_id();
  const std::string worker_id = worker_id_stream.str();

  SubmissionTask task{};
  while (queue_.pop(task)) {
    LOG_DEBUG("worker start worker_id=%s submission_id=%d reply_channel=%s",
              worker_id.c_str(), task.submission_id,
              task.reply_channel_id.c_str());

    if (notifier_ != nullptr && !stopped_.load()) {
      notifier_->onSubmissionStarted(task);
    }

    service_.processSubmission(task.submission_id, task.request);

    if (notifier_ != nullptr && !stopped_.load()) {
      notifier_->onSubmissionFinished(task);
    }

    LOG_DEBUG("worker finish worker_id=%s submission_id=%d reply_channel=%s",
              worker_id.c_str(), task.submission_id,
              task.reply_channel_id.c_str());
  }
}
