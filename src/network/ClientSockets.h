/**
 * @file ClientSockets.h
 * @brief TCP 连接管理与异步协议回推入口。
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <sys/select.h>
#include <vector>

#include "dispatch/SubmissionNotifier.h"
#include "dispatch/SubmissionQueue.h"
#include "network/ConnectionRegistry.h"
#include "network/QueryRequestHandler.h"
#include "network/AckBarrier.h"
#include "network/ReplyChannel.h"
#include "network/SubmissionEventResponder.h"
#include "network/SubmissionRequestHandler.h"
#include "protocol/JudgeProtocol.h"
#include "pipeline/SubmissionService.h"

class JudgeWorkerPool;

/**
 * @brief TCP 连接管理器，同时也是异步评测结果的 notifier 实现。
 *
 * 它负责：
 * - 新连接接入与槽位管理
 * - 收包、解帧、协议解析
 * - 创建 submission 并入队
 * - 立即返回 submission_ack
 * - 接收 worker 完成通知并把结果回推给连接
 */
class ClientSockets : public SubmissionNotifier {
public:
  using WakeCallback = std::function<void()>;

  /**
   * @brief 构造连接管理器。
   */
  ClientSockets(std::size_t slot_count, SubmissionQueue &submission_queue,
                SubmissionService &submission_service,
                WakeCallback wake_callback = nullptr);
  /** @brief 注册一个新的 client socket。 */
  void add_socket(int);

  /**
   * @brief 把当前所有活跃连接加入 read/write fd_set，并返回最大 fd。
   */
  int add_to_sets(fd_set &read_sets, fd_set &write_sets);

  /** @brief 根据逻辑槽位索引获得对应 fd。 */
  int id_to_fd(const int id) const { return connection_registry_.id_to_fd(id); }

  /**
   * @brief 处理一轮 select 返回的读写事件。
   */
  void deal_events(const fd_set &read_sets, const fd_set &write_sets);

  /** @brief 删除并回收一个连接槽位。 */
  void del_socket(int slot_id);

  /** @brief 暴露内部 SubmissionService，供 worker/notifier 协作使用。 */
  SubmissionService &submission_service() { return submission_service_; }

  /**
   * @brief 设置关联的 worker pool 指针，用于 graceful shutdown 时跳过回推。
   */
  void set_pool(JudgeWorkerPool *pool) { pool_ = pool; }

  /** @copydoc SubmissionNotifier::onSubmissionStarted */
  void onSubmissionStarted(const SubmissionTask &task) override;
  /** @copydoc SubmissionNotifier::onSubmissionFinished */
  void onSubmissionFinished(const SubmissionTask &task) override;

private:
  SubmissionQueue &submission_queue_;
  ConnectionRegistry connection_registry_;
  SubmissionService &submission_service_;
  JudgeProtocol protocol_;
  std::atomic<uint64_t> next_session_id_{1};
  AckBarrier ack_barrier_;
  SubmissionRequestHandler submission_request_handler_;
  QueryRequestHandler query_request_handler_;
  SubmissionEventResponder submission_event_responder_;

  /** @brief 把协议响应挂到目标连接的待发送队列。 */
  void queue_protocol_response_for_channel(const std::string &reply_channel_id,
                                           std::string response);
  /** @brief 处理 query_result 请求或回退为 bad request。 */
  void handle_query_or_bad_request(ConnectionSlot &slot,
                                   const std::string &message_body);
  /** @brief 解析并校验用于异步回推的 reply channel。 */
  std::optional<ReplyChannel>
  resolve_reply_channel_for_delivery(const std::string &reply_channel_id) const;
  /** @brief 触发外部 select 唤醒回调（若已配置）。 */
  void wake_select_loop();
  void handle_read_event(ConnectionSlot &slot, int slot_id);
  void handle_write_event(ConnectionSlot &slot, int slot_id);

  WakeCallback wake_callback_;
  JudgeWorkerPool *pool_{nullptr};
};
