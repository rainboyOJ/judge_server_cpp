/**
 * @file SubmissionRequestHandler.cpp
 * @brief submit 请求处理与 ack 顺序控制实现。
 */
#include "network/SubmissionRequestHandler.h"

#include "common/Logger.h"
#include "network/AckBarrier.h"
#include "pipeline/SubmissionService.h"
#include "protocol/JudgeProtocol.h"

SubmissionRequestHandler::SubmissionRequestHandler(
    SubmissionService &submission_service, JudgeProtocol &protocol,
    AckBarrier &ack_barrier)
    : submission_service_(submission_service), protocol_(protocol),
      ack_barrier_(ack_barrier) {}

SubmissionRequestHandler::HandleSubmitResult
SubmissionRequestHandler::submitEnsuringAckFirst(
    const SubmissionRequest &request, const std::string &reply_channel_id) {
  HandleSubmitResult result{};

  // 先标记“当前连接正在等待 ack”。
  // 这样即使 worker 很快产出异步状态消息，也会先被 AckBarrier 暂存，
  // 不会抢在 submission_ack 之前发给客户端。
  ack_barrier_.mark_waiting(reply_channel_id);

  const int submission_id =
      submission_service_.submitAsync(request, reply_channel_id);
  if (submission_id <= 0) {
    LOG_ERROR("submit async failed reply_channel=%s", reply_channel_id.c_str());
    result.response = protocol_.encodeError("submission queue unavailable");
    result.deferred_messages = ack_barrier_.release(reply_channel_id);
    return result;
  }
  LOG_DEBUG("submit queued submission_id=%d reply_channel=%s", submission_id,
            reply_channel_id.c_str());

  result.response = protocol_.encodeSubmissionAck(submission_id);
  // ack 已经生成完成，可以释放该连接在等待期间积压的异步消息。
  // 调用方会先发送 response，再继续发送 deferred_messages。
  result.deferred_messages = ack_barrier_.release(reply_channel_id);
  LOG_DEBUG("submit ack release submission_id=%d count=%zu", submission_id,
            result.deferred_messages.size());
  return result;
}
