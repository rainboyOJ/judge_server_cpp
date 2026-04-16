/**
 * @file SubmissionQueue.cpp
 * @brief 异步 submission 阻塞队列实现。
 */

#include "dispatch/SubmissionQueue.h"

#include <utility>

/** @copydoc SubmissionQueue::push */
bool SubmissionQueue::push(const SubmissionTask &task) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (shutdown_) {
    return false;
  }

  queue_.push(task);
  condition_.notify_one();
  return true;
}

/** @copydoc SubmissionQueue::pop */
bool SubmissionQueue::pop(SubmissionTask &task) {
  std::unique_lock<std::mutex> lock(mutex_);
  condition_.wait(lock, [this]() { return shutdown_ || !queue_.empty(); });

  if (queue_.empty()) {
    return false;
  }

  task = std::move(queue_.front());
  queue_.pop();
  return true;
}

/** @copydoc SubmissionQueue::shutdown */
void SubmissionQueue::shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  shutdown_ = true;
  condition_.notify_all();
}

/** @copydoc SubmissionQueue::size */
std::size_t SubmissionQueue::size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return queue_.size();
}
