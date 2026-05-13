#pragma once

#include <string>
#include <vector>

#include "common/SubmissionTypes.h"

class AckBarrier;
class JudgeProtocol;
class SubmissionService;

/**
 * @brief submit 请求处理器。
 *
 * 它负责创建 submission、投递任务，并保证 ack 与后续异步消息的顺序。
 */
class SubmissionRequestHandler {
public:
  /** @brief submit 处理结果：立即响应 + ack 后释放的积压消息。 */
  struct HandleSubmitResult {
    std::string response;
    std::vector<std::string> deferred_messages;
  };

  SubmissionRequestHandler(SubmissionService &submission_service,
                           JudgeProtocol &protocol, AckBarrier &ack_barrier);

  /** @brief 处理一条 submit 请求。 */
  HandleSubmitResult handleSubmit(const SubmissionRequest &request,
                                  const std::string &reply_channel_id);

private:
  SubmissionService &submission_service_;
  JudgeProtocol &protocol_;
  AckBarrier &ack_barrier_;
};
