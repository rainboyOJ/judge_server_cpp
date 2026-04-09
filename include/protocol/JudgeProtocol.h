#pragma once

#include <string>

#include "common/SubmissionTypes.h"

struct QueryResultRequest {
  int submission_id{};
};

class JudgeProtocol {
public:
  bool decodeRequest(const std::string &payload,
                     SubmissionRequest &request) const;
  bool decodeQueryRequest(const std::string &payload,
                          QueryResultRequest &request) const;
  std::string encodeSubmissionAck(int submission_id) const;
  std::string encodeSubmissionUpdate(const SubmissionResult &result) const;
  std::string encodeSubmissionFinished(const SubmissionResult &result) const;
  std::string encodeResult(const SubmissionResult &result) const;
  std::string encodeError(const std::string &message) const;
};
