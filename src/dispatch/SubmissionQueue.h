#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>

#include "dispatch/SubmissionTask.h"

/**
 * @brief 线程安全的 submission 阻塞队列。
 *
 * 生产者（网络线程）通过 push() 投递任务，
 * 消费者（worker 线程）通过 pop() 阻塞等待任务。
 */
class SubmissionQueue {
public:
  /**
   * @brief 向队列中投递一个 submission 任务。
   * @param task 待投递任务。
   * @return true 投递成功。
   * @return false 队列已 shutdown，拒绝新任务。
   */
  bool push(const SubmissionTask &task);

  /**
   * @brief 从队列中取出一个任务，必要时阻塞等待。
   * @param task 输出参数，成功时得到弹出的任务。
   * @return true 成功取到任务。
   * @return false 队列已关闭且没有剩余任务，消费者应退出。
   */
  bool pop(SubmissionTask &task);

  /**
   * @brief 关闭队列并唤醒所有阻塞中的消费者。
   */
  void shutdown();

  /**
   * @brief 返回当前队列中的任务数量。
   */
  std::size_t size() const;

private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::queue<SubmissionTask> queue_;
  bool shutdown_{false};
};
