#include "network/SubmissionRequestHandler.h"

#include "common/Logger.h"
#include "dispatch/SubmissionQueue.h"
#include "dispatch/SubmissionTask.h"
#include "network/AckBarrier.h"
#include "pipeline/SubmissionService.h"
#include "protocol/JudgeProtocol.h"

SubmissionRequestHandler::SubmissionRequestHandler(
    SubmissionQueue &submission_queue, SubmissionService &submission_service,
    JudgeProtocol &protocol, AckBarrier &ack_barrier)
    : submission_queue_(submission_queue),
      submission_service_(submission_service), protocol_(protocol),
      ack_barrier_(ack_barrier) {}

SubmissionRequestHandler::HandleSubmitResult SubmissionRequestHandler::handleSubmit(
    const SubmissionRequest &request, const std::string &reply_channel_id) {
  HandleSubmitResult result{};

  const int submission_id = submission_service_.createSubmission(request);
  if (submission_id <= 0) {
    LOG_ERROR("submit create failed reply_channel=%s",
              reply_channel_id.c_str());
    result.response = protocol_.encodeError("failed to create submission");
    return result;
  }
  LOG_DEBUG("submit created submission_id=%d reply_channel=%s", submission_id,
            reply_channel_id.c_str());

  SubmissionTask task{};
  task.submission_id = submission_id;
  task.request = request;
  task.reply_channel_id = reply_channel_id;

  ack_barrier_.mark_waiting(reply_channel_id);

  if (!submission_queue_.push(task)) {
    LOG_ERROR("submit queue failed submission_id=%d reply_channel=%s", submission_id,
              reply_channel_id.c_str());
    result.response = protocol_.encodeError("submission queue unavailable");
    result.deferred_messages = ack_barrier_.release(reply_channel_id);
    LOG_DEBUG("submit ack release submission_id=%d count=%zu", submission_id,
              result.deferred_messages.size());
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
