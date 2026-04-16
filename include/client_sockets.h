// 连接上层服务器主循环线程 与 testBox，
// 1. 加入，删除，client_socket
// 2. 设置socket的状态，可读，可写
// 3. 发送，读取，信息
// 4. 设置 外部的FD_SET
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Buffer.h" // 引入 Buffer 类
#include "dispatch/SubmissionNotifier.h"
#include "dispatch/SubmissionQueue.h"
#include "judge/JudgeCore.h"
#include "protocol/JudgeProtocol.h"
#include "runner/RunnerFactory.h"
#include "service/SubmissionService.h"
#include "store/ResultStore.h"
#include "testBox.h" //与testBox 建立连接

/**
 * @brief 单个连接在 select 循环中的当前收发状态。
 */
enum class FdInfoStatus {
  READABLE,
  WRITABLE,
  IDLE,
};

/**
 * @brief 单个客户端连接的运行时状态。
 *
 * 它封装了：
 * - socket fd
 * - 当前会话 id
 * - 输入缓冲区
 * - 待发送协议响应
 */
class FdInfo {
public:
  FdInfo() : fd(0), status(FdInfoStatus::READABLE) {}

  int get_fd() const { return fd; }
  uint64_t get_session_id() const { return session_id_; }

  void set_fd(int fd_) { fd = fd_; }

  /**
   * @brief 用新的 fd 和 session_id 重新初始化当前连接槽位。
   */
  void init(int fd_, uint64_t session_id) {
    // TODO 其时这里完全不用锁，因为根据这个函数使用时机
    std::lock_guard<std::mutex> lock(mtx_);
    fd = fd_;
    session_id_ = session_id;
    status = FdInfoStatus::READABLE;
    input_buffer_.retrieveAll(); // 清空缓冲区
    pending_response_.clear();
    pending_write_offset_ = 0;
  }

  /** @brief 关闭 fd 并清空当前槽位的状态。 */
  void clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (fd != 0)
      close(fd);
    fd = 0;
    session_id_ = 0;
    status = FdInfoStatus::READABLE;
    input_buffer_.retrieveAll(); // 清空缓冲区
    pending_response_.clear();
    pending_write_offset_ = 0;
  }

  /** @brief 当前连接是否应当被加入 read fd_set。 */
  bool is_readable() {
    std::lock_guard<std::mutex> lock(mtx_);
    return status == FdInfoStatus::READABLE;
  }
  /** @brief 当前连接是否应当被加入 write fd_set。 */
  bool is_writable() {
    std::lock_guard<std::mutex> lock(mtx_);
    return status == FdInfoStatus::WRITABLE;
  }

  /** @brief 将连接切换到可写状态。 */
  void set_writable() {
    std::lock_guard<std::mutex> lock(mtx_);
    status = FdInfoStatus::WRITABLE;
  }

  /** @brief 将连接切换回可读状态。 */
  void set_readable() {
    std::lock_guard<std::mutex> lock(mtx_);
    status = FdInfoStatus::READABLE;
  }

  /** @brief 将连接切换到空闲状态，等待下一轮事件重新驱动。 */
  void set_idle() {
    std::lock_guard<std::mutex> lock(mtx_);
    status = FdInfoStatus::IDLE;
  }

  /**
   * @brief 尝试从 socket 读取一个完整的 framed 消息。
   * @param read_size 本轮实际从 fd 读取到的字节数。
   * @param message_body 成功时得到去掉长度前缀的消息体。
   * @return true 已经成功解析出一条完整消息。
   * @return false 还未读满、连接关闭或读取出错。
   */
  bool read_message(int &read_size, std::string &message_body);

  /**
   * @brief 把当前待发送 buffer 的一部分写回 socket。
   * @return int 实际写出的字节数；若 < 0 则表示 send 出错。
   */
  int send(const std::string &result_data);

  /** @brief 设置完整待发送协议响应。 */
  void set_pending_response(std::string response) {
    std::lock_guard<std::mutex> lock(mtx_);
    queue_response_locked(std::move(response));
  }

  /**
   * @brief 只有 session 仍匹配时才设置待发送响应。
   *
   * 用来防止连接复用后，把旧 submission 的结果发给新连接。
   */
  bool set_pending_response_if_session(uint64_t expected_session_id,
                                       std::string response) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (session_id_ != expected_session_id || fd == 0) {
      return false;
    }

    queue_response_locked(std::move(response));
    status = FdInfoStatus::WRITABLE;
    return true;
  }

  /** @brief 当前连接是否还有未发完的协议响应。 */
  bool has_pending_response() {
    std::lock_guard<std::mutex> lock(mtx_);
    return !pending_response_.empty();
  }

  /** @brief 复制尚未发出的协议响应剩余部分。 */
  std::string copy_pending_response() {
    std::lock_guard<std::mutex> lock(mtx_);
    return pending_response_.substr(pending_write_offset_);
  }

  /**
   * @brief 消费掉已经发送完成的前缀部分。
   * @return true 本次调用后该响应已全部发完。
   * @return false 仍有剩余数据待发。
   */
  bool consume_pending_response(size_t bytes) {
    std::lock_guard<std::mutex> lock(mtx_);
    pending_write_offset_ += bytes;
    if (pending_write_offset_ >= pending_response_.size()) {
      pending_response_.clear();
      pending_write_offset_ = 0;
      return true;
    }
    return false;
  }

private:
  int fd;
  uint64_t session_id_{0};
  std::mutex mtx_;
  Buffer input_buffer_; // 为每个FdInfo添加一个输入缓冲区
  FdInfoStatus status;
  std::string pending_response_;
  size_t pending_write_offset_{0};

  /**
   * @brief 在持锁状态下把响应封装成“4 字节长度 + body”的完整 frame。
   */
  void queue_response_locked(std::string response) {
    const uint32_t body_size = htonl(static_cast<uint32_t>(response.size()));
    pending_response_.append(reinterpret_cast<const char *>(&body_size),
                             sizeof(body_size));
    pending_response_ += response;
  }
};

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
  /**
   * @brief 构造连接管理器。
   */
  ClientSockets(testBox *test_box, SubmissionQueue &submission_queue);
  /** @brief 注册一个新的 client socket。 */
  void add_socket(int);

  /**
   * @brief 把当前所有活跃连接加入 read/write fd_set，并返回最大 fd。
   */
  int add_to_sets(fd_set &read_sets, fd_set &write_sets);

  /** @brief 根据 testBox 槽位索引获得对应 fd。 */
  int id_to_fd(const int id) const { return client_sockets_[id]->get_fd(); };

  /**
   * @brief 处理一轮 select 返回的读写事件。
   */
  void deal_events(const fd_set &read_sets, const fd_set &write_sets);

  /** @brief 删除并回收一个连接槽位。 */
  void del_socket(int testBoxId);

  /** @brief 暴露内部 SubmissionService，供 worker/notifier 协作使用。 */
  SubmissionService &submission_service() { return submission_service_; }

  /** @copydoc SubmissionNotifier::onSubmissionStarted */
  void onSubmissionStarted(const SubmissionTask &task) override;
  /** @copydoc SubmissionNotifier::onSubmissionFinished */
  void onSubmissionFinished(const SubmissionTask &task) override;

private:
  /** @brief 旧接口保留：读取 socket 数据。当前主链路已优先走 read_message。 */
  int read_socket(int testBoxId, FdInfo &fd_info);
  /** @brief 旧接口保留：发送 socket 数据。当前主链路已优先走 pending_response。 */
  int send_socket(int testBoxId, FdInfo &fd_info);

  testBox *test_box_; // 指向testBox的指针
  SubmissionQueue &submission_queue_;
  std::vector<std::unique_ptr<FdInfo>> client_sockets_; // 保存所有的socket信息
  ResultStore result_store_;
  RunnerFactory runner_factory_;
  JudgeCore judge_core_;
  SubmissionService submission_service_;
  JudgeProtocol protocol_;
  std::atomic<uint64_t> next_session_id_{1};
  mutable std::mutex notifier_mutex_;
  std::unordered_set<std::string> awaiting_ack_channels_;
  std::unordered_map<std::string, std::vector<std::string>>
      deferred_protocol_messages_;

  /** @brief 生成“槽位 + 会话”组成的异步回推通道标识。 */
  std::string make_reply_channel_id(int testBoxId, uint64_t session_id) const;
  /** @brief 解析 reply_channel_id 得到目标槽位和会话 id。 */
  bool parse_reply_channel_id(const std::string &reply_channel_id,
                              int &testBoxId, uint64_t &session_id) const;
  /** @brief 把协议响应挂到目标连接的待发送队列。 */
  void queue_protocol_response_for_channel(const std::string &reply_channel_id,
                                           std::string response);
  /** @brief 标记某个 channel 目前必须先发 submission_ack。 */
  void mark_channel_waiting_for_ack(const std::string &reply_channel_id);
  /** @brief 解除 ack 屏障，并释放该 channel 暂存的后续消息。 */
  void mark_channel_ack_sent(const std::string &reply_channel_id);
};
