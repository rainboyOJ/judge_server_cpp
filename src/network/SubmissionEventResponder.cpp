#include "network/SubmissionEventResponder.h"

#include "pipeline/SubmissionService.h"
#include "protocol/JudgeProtocol.h"

SubmissionEventResponder::SubmissionEventResponder(
    SubmissionService &submission_service, JudgeProtocol &protocol)
    : submission_service_(submission_service), protocol_(protocol) {}

std::optional<std::string>
SubmissionEventResponder::onSubmissionStarted(const SubmissionTask &task) {
  SubmissionResult result{};
  if (!submission_service_.query(task.submission_id, result)) {
    return std::nullopt;
  }

  if (result.status == SubmissionStatus::QUEUED) {
    return std::nullopt;
  }

  return protocol_.encodeSubmissionUpdate(result);
}

std::optional<std::string>
SubmissionEventResponder::onSubmissionFinished(const SubmissionTask &task) {
  SubmissionResult result{};
  if (!submission_service_.query(task.submission_id, result)) {
    return std::nullopt;
  }

  return protocol_.encodeResult(result);
}
