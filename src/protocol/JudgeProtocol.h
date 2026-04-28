#pragma once

#include <string>

#include "common/SubmissionTypes.h"

/**
 * @brief query_result 请求解码后的内部对象。
 */
struct QueryResultRequest {
  int submission_id{};
};

/**
 * @brief 负责异步评测协议的 JSON 编解码。
 *
 * 当前协议主要支持：
 * - submit
 * - query_result
 * - submission_ack
 * - submission_update
 * - submission_finished
 */
class JudgeProtocol {
public:
  /**
   * @brief 将 submit 请求 JSON 解码为 SubmissionRequest。
   */
  bool decodeRequest(const std::string &payload,
                     SubmissionRequest &request) const;
  /**
   * @brief 将 query_result 请求 JSON 解码为 QueryResultRequest。
   */
  bool decodeQueryRequest(const std::string &payload,
                          QueryResultRequest &request) const;
  /**
   * @brief 编码提交已入队的确认消息。
   */
  std::string encodeSubmissionAck(int submission_id) const;
  /**
   * @brief 编码非终态 submission 的更新消息。
   */
  std::string encodeSubmissionUpdate(const SubmissionResult &result) const;
  /**
   * @brief 编码终态 submission 的完成消息。
   */
  std::string encodeSubmissionFinished(const SubmissionResult &result) const;
  /**
   * @brief 根据快照状态自动选择 update 或 finished 编码方式。
   */
  std::string encodeResult(const SubmissionResult &result) const;
  /**
   * @brief 编码错误响应。
   */
  std::string encodeError(const std::string &message) const;
};
