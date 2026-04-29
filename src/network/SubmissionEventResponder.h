#pragma once

#include <optional>
#include <string>

#include "dispatch/SubmissionTask.h"

class JudgeProtocol;
class SubmissionService;

/**
 * @brief 把 worker 生命周期事件翻译成协议消息。
 */
class SubmissionEventResponder {
public:
  SubmissionEventResponder(SubmissionService &submission_service,
                           JudgeProtocol &protocol);

  /** @brief 处理 started 事件，必要时返回 `submission_update`。 */
  std::optional<std::string>
  onSubmissionStarted(const SubmissionTask &task);

  /** @brief 处理 finished 事件，返回 update 或 finished 消息。 */
  std::optional<std::string>
  onSubmissionFinished(const SubmissionTask &task);

private:
  SubmissionService &submission_service_;
  JudgeProtocol &protocol_;
};
