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

enum class FdInfoStatus {
  READABLE,
  WRITABLE,
  IDLE,
};

class FdInfo {
public:
  FdInfo() : fd(0), status(FdInfoStatus::READABLE) {}

  int get_fd() const { return fd; }
  uint64_t get_session_id() const { return session_id_; }

  void set_fd(int fd_) { fd = fd_; }

  // 初始化
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

  bool is_readable() {
    std::lock_guard<std::mutex> lock(mtx_);
    return status == FdInfoStatus::READABLE;
  }
  bool is_writable() {
    std::lock_guard<std::mutex> lock(mtx_);
    return status == FdInfoStatus::WRITABLE;
  }

  void set_writable() {
    std::lock_guard<std::mutex> lock(mtx_);
    status = FdInfoStatus::WRITABLE;
  }

  void set_readable() {
    std::lock_guard<std::mutex> lock(mtx_);
    status = FdInfoStatus::READABLE;
  }

  void set_idle() {
    std::lock_guard<std::mutex> lock(mtx_);
    status = FdInfoStatus::IDLE;
  }

  // 读取数据
  bool read_message(int &read_size, std::string &message_body);

  // 发送数据
  int send(const std::string &result_data);

  void set_pending_response(std::string response) {
    std::lock_guard<std::mutex> lock(mtx_);
    queue_response_locked(std::move(response));
  }

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

  bool has_pending_response() {
    std::lock_guard<std::mutex> lock(mtx_);
    return !pending_response_.empty();
  }

  std::string copy_pending_response() {
    std::lock_guard<std::mutex> lock(mtx_);
    return pending_response_.substr(pending_write_offset_);
  }

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

  void queue_response_locked(std::string response) {
    const uint32_t body_size = htonl(static_cast<uint32_t>(response.size()));
    pending_response_.append(reinterpret_cast<const char *>(&body_size),
                             sizeof(body_size));
    pending_response_ += response;
  }
};

class ClientSockets : public SubmissionNotifier {
public:
  ClientSockets(testBox *test_box, SubmissionQueue &submission_queue);
  void add_socket(int);

  // 设置所有socket的加入对应的监听，并返回最大的那个socket
  int add_to_sets(fd_set &read_sets, fd_set &write_sets);

  // 得到testBoxId对应的fd
  int id_to_fd(const int id) const { return client_sockets_[id]->get_fd(); };

  // 处理对应的事件
  void deal_events(const fd_set &read_sets, const fd_set &write_sets);

  // 删除一个socket
  void del_socket(int testBoxId);

  SubmissionService &submission_service() { return submission_service_; }

  void onSubmissionStarted(const SubmissionTask &task) override;
  void onSubmissionFinished(const SubmissionTask &task) override;

private:
  // 读取发来的评测信息
  int read_socket(int testBoxId, FdInfo &fd_info);
  // 发送评测信息
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

  std::string make_reply_channel_id(int testBoxId, uint64_t session_id) const;
  bool parse_reply_channel_id(const std::string &reply_channel_id,
                              int &testBoxId, uint64_t &session_id) const;
  void queue_protocol_response_for_channel(const std::string &reply_channel_id,
                                           std::string response);
  void mark_channel_waiting_for_ack(const std::string &reply_channel_id);
  void mark_channel_ack_sent(const std::string &reply_channel_id);
};
