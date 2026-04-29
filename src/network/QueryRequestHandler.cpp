#include "network/QueryRequestHandler.h"

#include "pipeline/SubmissionService.h"

QueryRequestHandler::QueryRequestHandler(SubmissionService &submission_service,
                                         JudgeProtocol &protocol)
    : submission_service_(submission_service), protocol_(protocol) {}

std::string QueryRequestHandler::handleQuery(
    const QueryResultRequest &request) {
  SubmissionResult result{};
  if (!submission_service_.query(request.submission_id, result)) {
    return protocol_.encodeError("submission not found");
  }

  return protocol_.encodeResult(result);
}
