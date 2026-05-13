#pragma once

#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

#include "dispatch/SubmissionNotifier.h"

class SubmissionService;

/**
 * @brief 后台异步评测 worker 线程池。
 *
 * 它持续从 @ref SubmissionService 内部队列中消费 @ref SubmissionTask，
 * 再调用 @ref SubmissionService::processSubmission 执行完整评测流程。
 */
class JudgeWorkerPool {
public:
  /**
   * @brief 构造并立即启动固定数量的 worker 线程。
   */
  JudgeWorkerPool(std::size_t worker_count, SubmissionService &service,
                  SubmissionNotifier *notifier = nullptr);
  ~JudgeWorkerPool();

  /**
   * @brief 停止线程池，关闭队列并等待所有 worker 退出。
   */
  void stop();

  /**
   * @brief 优雅关闭线程池，先标记停止状态再关闭队列。
   */
  void graceful_shutdown();

  /**
   * @brief 查询是否正在停止中。
   */
  bool is_stopping() const { return stopped_.load(); }

private:
  /**
   * @brief 单个 worker 线程的主循环。
   */
  void workerLoop();

  SubmissionService &service_;
  SubmissionNotifier *notifier_;
  std::vector<std::thread> workers_;
  std::atomic<bool> stopped_{false};
};
