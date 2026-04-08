#pragma once

#include <string>

#include "common/SubmissionTypes.h"

// 协议层只负责网络载荷与内部结构之间的转换，不参与判题流程编排。
// 当前阶段优先兼容旧客户端已经在使用的 JSON 请求字段：uuid/pid/lang/code。
class JudgeProtocol {
  public:
    bool decodeRequest(const std::string &payload,
                       SubmissionRequest &request) const;
    std::string encodeResult(const SubmissionResult &result) const;
    std::string encodeError(const std::string &message) const;
};
