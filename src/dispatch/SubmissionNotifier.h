#pragma once

#include "dispatch/SubmissionTask.h"

/**
 * @brief Worker 生命周期事件通知接口。
 *
 * 调度层不直接操作 socket 或协议，而是通过这个接口把“某个 submission
 * 开始/结束执行”的事实通知给上层（当前由 @ref ClientSockets 实现）。
 */
class SubmissionNotifier {
public:
  virtual ~SubmissionNotifier() = default;

  /**
   * @brief 当 worker 开始处理一个 submission 时调用。
   */
  virtual void onSubmissionStarted(const SubmissionTask &task) { (void)task; }

  /**
   * @brief 当 worker 完成一个 submission 的处理时调用。
   */
  virtual void onSubmissionFinished(const SubmissionTask &task) { (void)task; }
};
