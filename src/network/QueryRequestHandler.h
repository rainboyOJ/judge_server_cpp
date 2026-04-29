#pragma once

#include <string>

#include "protocol/JudgeProtocol.h"

class SubmissionService;

/**
 * @brief `query_result` 请求处理器。
 */
class QueryRequestHandler {
public:
  QueryRequestHandler(SubmissionService &submission_service,
                      JudgeProtocol &protocol);

  /** @brief 查询某个 submission 的最新结果快照并编码成协议响应。 */
  std::string handleQuery(const QueryResultRequest &request);

private:
  SubmissionService &submission_service_;
  JudgeProtocol &protocol_;
};
