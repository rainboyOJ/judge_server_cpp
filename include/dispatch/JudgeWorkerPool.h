#pragma once

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

#include "dispatch/SubmissionNotifier.h"
#include "dispatch/SubmissionQueue.h"

class SubmissionService;

class JudgeWorkerPool {
public:
  JudgeWorkerPool(std::size_t worker_count, SubmissionQueue &queue,
                  SubmissionService &service,
                  SubmissionNotifier *notifier = nullptr);
  ~JudgeWorkerPool();

  void stop();

private:
  void workerLoop();

  SubmissionQueue &queue_;
  SubmissionService &service_;
  SubmissionNotifier *notifier_;
  std::vector<std::thread> workers_;
  std::atomic<bool> stopped_{false};
};
