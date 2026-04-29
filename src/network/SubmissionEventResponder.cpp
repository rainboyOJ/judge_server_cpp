#include "network/SubmissionEventResponder.h"

#include "common/Logger.h"
#include "pipeline/SubmissionService.h"
#include "protocol/JudgeProtocol.h"

SubmissionEventResponder::SubmissionEventResponder(
    SubmissionService &submission_service, JudgeProtocol &protocol)
    : submission_service_(submission_service), protocol_(protocol) {}

std::optional<std::string>
SubmissionEventResponder::onSubmissionStarted(const SubmissionTask &task) {
  SubmissionResult result{};
  if (!submission_service_.query(task.submission_id, result)) {
    LOG_DEBUG("async start miss id=%d", task.submission_id);
    return std::nullopt;
  }

  if (result.status == SubmissionStatus::QUEUED) {
    LOG_DEBUG("async start skip queued id=%d", task.submission_id);
    return std::nullopt;
  }

  LOG_DEBUG("async start msg id=%d status=%d", task.submission_id,
            static_cast<int>(result.status));
  return protocol_.encodeSubmissionUpdate(result);
}

std::optional<std::string>
SubmissionEventResponder::onSubmissionFinished(const SubmissionTask &task) {
  SubmissionResult result{};
  if (!submission_service_.query(task.submission_id, result)) {
    LOG_DEBUG("async finish miss id=%d", task.submission_id);
    return std::nullopt;
  }

  const char *message_type = result.status == SubmissionStatus::FINISHED ||
                                     result.status == SubmissionStatus::FAILED
                                 ? "submission_finished"
                                 : "submission_update";
  LOG_DEBUG("async finish msg id=%d status=%d type=%s", task.submission_id,
            static_cast<int>(result.status), message_type);
  return protocol_.encodeResult(result);
}
