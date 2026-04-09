#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

#include "dispatch/SubmissionTask.h"

class SubmissionQueue {
public:
  bool push(const SubmissionTask &task);
  bool pop(SubmissionTask &task);
  void shutdown();
  std::size_t size() const;

private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::queue<SubmissionTask> queue_;
  bool shutdown_{false};
};
