#pragma once

#include <string>

#include "common/SubmissionTypes.h"

/**
 * @brief SubmissionQueue 中流转的基本任务单元。
 *
 * 这个结构把一次待执行的 submission 以及回推通道信息打包在一起，
 * 供 @ref JudgeWorkerPool 后台消费。
 */
struct SubmissionTask {
  /** 服务端生成的 submission 唯一编号。 */
  int submission_id{-1};
  /** 原始提交请求内容。 */
  SubmissionRequest request;
  /** 用于将异步结果回推给正确连接的逻辑通道标识。 */
  std::string reply_channel_id;
};
