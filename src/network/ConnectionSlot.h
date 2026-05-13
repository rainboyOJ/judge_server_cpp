/**
 * @file ConnectionSlot.h
 * @brief 单个TCP客户端连接槽位的状态、缓冲和会话管理。
 *
 * 每个 ConnectionSlot 封装了一个客户端连接的完整运行时状态：
 * - socket fd 与 session id（防止 fd 复用后错发旧消息）
 * - TCP framing 输入缓冲区（4 字节长度前缀 + JSON body）
 * - 待发送协议响应缓冲区（同样带 4 字节长度前缀）
 * - select 循环中的读写状态（READABLE / WRITABLE / IDLE）
 *
 * 线程安全：除 read_message() 仅在 select 主线程调用外，
 * 其余 getter/setter 均通过内部 mutex 保护，
 * 允许后台 worker 线程安全地设置待发送响应。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

#include <netinet/in.h>
#include <unistd.h>

#include "network/Buffer.h"

/**
 * @brief 连接槽位在 select 循环中的当前收发状态。
 *
 * - READABLE：应加入 read_fd_set，等待客户端新数据到达。
 * - WRITABLE：应加入 write_fd_set，当前有待发送响应需要刷出。
 * - IDLE：当前既无待读数据也无待发数据，本轮不参与 select。
 */
enum class ConnectionSlotStatus {
  READABLE,
  WRITABLE,
  IDLE,
};

/**
 * @brief 单个客户端连接的运行时状态管理器。
 *
 * 它封装了：
 * - socket fd 与 session id
 * - 输入缓冲区（TCP framing 解析）
 * - 待发送协议响应（已编码为 4 字节长度前缀 + body 的完整 frame）
 * - select 循环读写状态
 *
 * 所有公开方法除 read_message() 外均持有内部锁，
 * 因此后台 worker 线程可以安全地通过
 * set_pending_response_if_session() 把异步评测结果挂到连接上。
 */
class ConnectionSlot {
public:
  ConnectionSlot() = default;

  /** @brief 获取当前 socket fd，0 表示槽位空闲。 */
  int get_fd() const { return fd_; }

  /** @brief 获取当前会话 id，用于防止 fd 复用后错发消息。 */
  uint64_t get_session_id() const { return session_id_; }

  /**
   * @brief 用新的 fd 和 session_id 初始化槽位。
   *
   * 通常在 ClientSockets::register_client_socket() 中调用。
   * 会清空所有旧状态（输入缓冲、待发送响应、读写偏移）。
   */
  void init(int fd, uint64_t session_id);

  /**
   * @brief 关闭 fd 并清空槽位状态，使槽位可被复用。
   */
  void clear();

  /** @brief 当前是否应加入 read_fd_set。 */
  bool is_readable();

  /** @brief 当前是否应加入 write_fd_set。 */
  bool is_writable();

  /** @brief 将槽位切到可写状态（有待发送数据）。 */

  void set_writable();

  /** @brief 将槽位切回可读状态（发送完成）。 */
  void set_readable();

  /**
   * @brief 将槽位切到空闲状态。
   *
   * 通常在一轮事件处理完成后调用，表示本轮不再需要 select 关注。
   */
  void set_idle();

  /**
   * @brief 尝试从 fd 读取一个完整 framed 消息。
   * @param read_size 输出：本轮实际从 socket 读取的字节数（含长度前缀）。
   * @param message_body 输出：去掉长度前缀后的完整 JSON 消息体。
   * @return true 已成功解析出一条完整消息。
   * @return false 数据未读完、连接关闭或读取出错。
   *
   * 该函数仅在 select 主线程调用，不加锁。
   */
  bool read_message(int &read_size, std::string &message_body);

  /**
   * @brief 把一段二进制数据写回 socket。
   * @return int 实际写出的字节数，<0 表示 send 出错。
   */
  int send(const std::string &result_data);

  /**
   * @brief 设置待发送协议响应（已包含 4 字节长度前缀的完整 frame）。
   */
  void set_pending_response(std::string response);

  /**
   * @brief 仅当当前 session 匹配时才设置待发送响应。
   * @param expected_session_id 期望的会话 id。
   * @param response 待发送的完整 frame。
   * @return true 设置成功（session 匹配且 fd 非零）。
   * @return false session 不匹配或 fd 已失效。
   *
   * 这是后台 worker 线程安全挂载异步评测结果的关键入口：
   * 它保证旧 submission 的结果不会因为 fd 复用而发给新连接。
   */
  bool set_pending_response_if_session(uint64_t expected_session_id,
                                       std::string response);

  /** @brief 当前是否还有未发完的协议响应。 */
  bool has_pending_response();

  /** @brief 复制尚未发完的剩余部分（不移除）。 */
  std::string copy_pending_response();

  /**
   * @brief 消费掉已发送完成的前缀。
   * @return true 本次调用后该响应已全部发完。
   * @return false 仍有剩余数据待下一轮继续发送。
   */
  bool consume_pending_response(std::size_t bytes);

private:
  int fd_{0};
  uint64_t session_id_{0};
  std::mutex mutex_;
  Buffer input_buffer_;
  ConnectionSlotStatus status_{ConnectionSlotStatus::READABLE};
  std::string pending_response_;
  std::size_t pending_write_offset_{0};

  /**
   * @brief 在持锁状态下把响应封装成 4 字节长度前缀 + body 的完整 frame。
   */
  void queue_response_locked(std::string response);
};
