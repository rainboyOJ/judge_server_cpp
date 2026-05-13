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
SubmissionRequestHandler::handleSubmit(const SubmissionRequest &request,
                                       const std::string &reply_channel_id) {
  HandleSubmitResult result{};

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
  result.deferred_messages = ack_barrier_.release(reply_channel_id);
  LOG_DEBUG("submit ack release submission_id=%d count=%zu", submission_id,
            result.deferred_messages.size());
  return result;
}
