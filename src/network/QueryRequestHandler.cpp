#include "network/QueryRequestHandler.h"

#include "common/Logger.h"
#include "pipeline/SubmissionService.h"

QueryRequestHandler::QueryRequestHandler(SubmissionService &submission_service,
                                         JudgeProtocol &protocol)
    : submission_service_(submission_service), protocol_(protocol) {}

std::string QueryRequestHandler::handleQuery(
    const QueryResultRequest &request) {
  SubmissionResult result{};
  if (!submission_service_.query(request.submission_id, result)) {
    LOG_DEBUG("query miss submission_id=%d", request.submission_id);
    return protocol_.encodeError("submission not found");
  }

  LOG_DEBUG("query hit submission_id=%d status=%d", request.submission_id,
            static_cast<int>(result.status));
  return protocol_.encodeResult(result);
}
