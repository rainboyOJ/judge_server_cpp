// 连接上层服务器主循环线程 与 testBox，
// 1. 加入，删除，client_socket
// 2. 设置socket的状态，可读，可写
// 3. 发送，读取，信息
// 4. 设置 外部的FD_SET
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <sys/select.h>
#include <vector>

#include "dispatch/SubmissionNotifier.h"
#include "dispatch/SubmissionQueue.h"
#include "pipeline/JudgeCore.h"
#include "network/ConnectionRegistry.h"
#include "network/AckBarrier.h"
#include "protocol/JudgeProtocol.h"
#include "runner/RunnerFactory.h"
#include "pipeline/SubmissionService.h"
#include "pipeline/ResultStore.h"

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
  ResultStore result_store_;
  RunnerFactory runner_factory_;
  JudgeCore judge_core_;
  SubmissionService submission_service_;
  JudgeProtocol protocol_;
  std::atomic<uint64_t> next_session_id_{1};
  AckBarrier ack_barrier_;

  /** @brief 生成“槽位 + 会话”组成的异步回推通道标识。 */
  std::string make_reply_channel_id(int testBoxId, uint64_t session_id) const;
  /** @brief 解析 reply_channel_id 得到目标槽位和会话 id。 */
  bool parse_reply_channel_id(const std::string &reply_channel_id,
                              int &testBoxId, uint64_t &session_id) const;
  /** @brief 把协议响应挂到目标连接的待发送队列。 */
  void queue_protocol_response_for_channel(const std::string &reply_channel_id,
                                           std::string response);
  /** @brief 触发外部 select 唤醒回调（若已配置）。 */
  void wake_select_loop();

  WakeCallback wake_callback_;
  JudgeWorkerPool *pool_{nullptr};
};
