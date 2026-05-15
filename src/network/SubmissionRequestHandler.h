/**
 * @file SubmissionRequestHandler.h
 * @brief submit 请求到 SubmissionService 的适配与 ack 顺序协调。
 */
#pragma once

#include <string>
#include <vector>

#include "common/SubmissionTypes.h"

class AckBarrier;
class JudgeProtocol;
class SubmissionService;

/**
 * @brief submit 请求处理器。
 *
 * 它位于协议层和 SubmissionService 之间，负责：
 * - 调用 SubmissionService 创建 submission 并投递异步任务
 * - 生成当前连接的立即响应（submission_ack 或 error）
 * - 借助 AckBarrier 保证 submission_ack 一定先于后续异步状态消息发出
 */
class SubmissionRequestHandler {
public:
  /**
   * @brief submit 处理结果。
   *
   * response 是当前连接要立即返回的协议消息；
   * deferred_messages 是 ack 发送完成后才允许继续刷出的异步消息。
   */
  struct HandleSubmitResult {
    std::string response;
    std::vector<std::string> deferred_messages;
  };

  /** @brief 构造 submit 请求处理器。 */
  SubmissionRequestHandler(SubmissionService &submission_service,
                           JudgeProtocol &protocol, AckBarrier &ack_barrier);

  /**
   * @brief 处理一条 submit 请求。
   *
   * 调用顺序很关键：
   * - 先通过 AckBarrier::mark_waiting() 把当前 reply channel 标记为“等待 ack”
   * - 再调用 SubmissionService::submitAsync() 把任务放入后台队列
   * - JudgeWorkerPool 的 worker 线程随后会通过 waitTask()/queue.pop() 取走任务
   * - 若 worker 在 ack 发出前产生异步状态消息，这些消息会在 ClientSockets 中被
   *   AckBarrier::try_defer() 暂存
   * - 当前函数生成 ack 后再 release()，调用方据此保证 ack 先发、异步消息后发
   *
   * @param request 已解码的提交请求。
   * @param reply_channel_id 用于后续异步回推结果的 reply channel 标识。
   * @return HandleSubmitResult 立即响应与可释放的延迟消息集合。
   */
  HandleSubmitResult
  submitEnsuringAckFirst(const SubmissionRequest &request,
                         const std::string &reply_channel_id);

private:
  SubmissionService &submission_service_;
  JudgeProtocol &protocol_;
  AckBarrier &ack_barrier_;
};
