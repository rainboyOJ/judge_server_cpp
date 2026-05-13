/**
 * @file ClientSockets.cpp
 * @brief TCP 连接管理、协议接入与异步结果回推实现。
 */

#include <arpa/inet.h>
#include <sys/select.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <cctype>
#include <exception>
#include <memory>
#include <string>

#include "common/Logger.h"
#include "dispatch/JudgeWorkerPool.h"
#include "network/ClientSockets.h"
#include "network/ReplyChannel.h"
#include "protocol/JudgeProtocol.h"

namespace {

std::string make_protocol_frame(const std::string &body) {
  const uint32_t body_size = htonl(static_cast<uint32_t>(body.size()));

  std::string frame;
  frame.append(reinterpret_cast<const char *>(&body_size), sizeof(body_size));
  frame += body;
  return frame;
}

void send_connection_limit_error_and_close(int client_socket,
                                           const JudgeProtocol &protocol) {
  const std::string response =
      protocol.encodeError("judge server is busy, too many active connections");
  const std::string frame = make_protocol_frame(response);

  std::size_t written = 0;
  while (written < frame.size()) {
    const ssize_t bytes =
        write(client_socket, frame.data() + written, frame.size() - written);
    if (bytes > 0) {
      written += static_cast<std::size_t>(bytes);
      continue;
    }

    if (bytes < 0 && errno == EINTR) {
      continue;
    }

    break;
  }

  if (written < frame.size()) {
    LOG_DEBUG("connection limit error response partially sent to socket %d, "
              "written=%zu, expected=%zu, errno=%d",
              client_socket, written, frame.size(), errno);
  }

  close(client_socket);
}

/**
 * @brief 清理用于日志输出的题目/提交标识，避免异常字符污染日志格式。
 *
 * pid 来自客户端请求，理论上可以包含任意字符。日志里直接输出原始 pid，
 * 可能导致换行、控制字符或分隔符把一条日志拆乱，影响排查问题。这里保留
 * 常见的字母、数字和少量安全分隔符，其它字符统一替换成 '_'，并限制最大
 * 输出长度，保证日志可读且不会被超长字段刷屏。
 */
std::string sanitize_pid_for_log(const std::string &pid) {
  constexpr std::size_t kMaxPidLogLength = 32;

  std::string sanitized = pid;
  for (char &ch : sanitized) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!(std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.' ||
          ch == ':')) {
      ch = '_';
    }
  }

  if (sanitized.size() > kMaxPidLogLength) {
    sanitized.resize(kMaxPidLogLength);
    sanitized += "...";
  }

  return sanitized;
}

} // namespace

/** @copydoc ClientSockets::ClientSockets */
ClientSockets::ClientSockets(std::size_t slot_count,
                             SubmissionService &submission_service,
                             WakeCallback wake_callback)
    : connection_registry_(slot_count), submission_service_(submission_service),
      submission_request_handler_(submission_service_, protocol_, ack_barrier_),
      query_request_handler_(submission_service_, protocol_),
      submission_event_responder_(submission_service_, protocol_),
      wake_callback_(std::move(wake_callback)) {}

/** @copydoc ClientSockets::populate_socket_sets */
int ClientSockets::populate_socket_sets(fd_set &read_sets, fd_set &write_sets) {
  return connection_registry_.populate_socket_sets(read_sets, write_sets);
}

/** @copydoc ClientSockets::register_client_socket */
void ClientSockets::register_client_socket(int client_socket) {
  const int slot_id =
      connection_registry_.acquire_slot(client_socket, next_session_id_++);
  if (slot_id < 0) {
    LOG_DEBUG("connection registry is FULL, disconnect socket %d",
              client_socket);
    send_connection_limit_error_and_close(client_socket, protocol_);
  } else {
    LOG_DEBUG("add socket %d to logical slot %d", client_socket, slot_id);
  }
}

/** @copydoc ClientSockets::release_connection */
void ClientSockets::release_connection(int slot_id) {
  connection_registry_.release_slot(static_cast<std::size_t>(slot_id));
}

/** @copydoc ClientSockets::handle_ready_events */
void ClientSockets::handle_ready_events(const fd_set &read_sets,
                                const fd_set &write_sets) {

  for (std::size_t i = 0; i < connection_registry_.size(); i++) {
    ConnectionSlot &slot = connection_registry_.slot(i);
    int client_socket = slot.get_fd();
    if (client_socket == 0)
      continue;
    int slot_id = static_cast<int>(i);
    if (FD_ISSET(client_socket, &read_sets)) {
      handle_read_event(slot, slot_id);
    } else if (FD_ISSET(client_socket, &write_sets)) {
      handle_write_event(slot, slot_id);
    }
  }
}

void ClientSockets::handle_read_event(ConnectionSlot &slot, int slot_id) {
  int client_socket = slot.get_fd();
  LOG_DEBUG("read event on socket %d\n", client_socket);
  int bytes_read = 0;
  std::string message_body;
  const bool has_complete_message = slot.read_message(bytes_read, message_body);
  if (bytes_read == 0) {
    LOG_INFO("Connection closed : %d\n", client_socket);
    release_connection(slot_id);
  } else if (bytes_read < 0) {
    LOG_ERROR("Failed to read from socket: %d\n", client_socket);
    release_connection(slot_id);
  } else if (has_complete_message) { // 没有发生错误
    handle_complete_message(slot, slot_id, message_body);
  } // 没有读取发生错误else end
  else {
    LOG_DEBUG("read bytes_read = %d, but not get complete message",
              bytes_read);
  }
}

void ClientSockets::handle_write_event(ConnectionSlot &slot, int slot_id) {
  int client_socket = slot.get_fd();
  LOG_DEBUG("write event on socket %d\n", client_socket);
  // 1. 检查socket状态，确保可写
  if (!slot.is_writable()) // 不是可写状态
    return;

  if (!slot.has_pending_response()) {
    slot.set_readable();
    return;
  }
  std::string result_data = slot.copy_pending_response();
#ifdef MUDEBUG
  // LOG_DEBUG("send %d bytes to socket %d", result_data.size(),
  // client_socket); debug_print_uint8_t_vector(result_data);
#endif
  int bytes_write = slot.send(result_data);
  if (bytes_write < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    LOG_ERROR("Failed to send response to socket %d", client_socket);
    release_connection(slot_id);
    return;
  }

  if (!slot.consume_pending_response(bytes_write)) {
    return;
  }

  slot.set_readable(); // 转入readable 的状态
}

void ClientSockets::handle_query_or_bad_request(
    ConnectionSlot &slot, const std::string &message_body) {
  QueryResultRequest query_request{};
  if (protocol_.decodeQueryRequest(message_body, query_request)) {
    handle_query_message(slot, query_request);
  } else {
    queue_immediate_response(slot, protocol_.encodeError("bad request"));
  }
}

void ClientSockets::queue_immediate_response(ConnectionSlot &slot,
                                             std::string response) {
  slot.set_pending_response(std::move(response));
  slot.set_writable();
}

void ClientSockets::handle_complete_message(ConnectionSlot &slot, int slot_id,
                                            const std::string &message_body) {
  SubmissionRequest request{};
  if (protocol_.decodeRequest(message_body, request)) {
    handle_submit_message(slot, slot_id, request);
    return;
  }

  handle_query_or_bad_request(slot, message_body);
}

void ClientSockets::handle_submit_message(ConnectionSlot &slot, int slot_id,
                                          const SubmissionRequest &request) {
  const std::string reply_channel_id =
      ReplyChannel{slot_id, slot.get_session_id()}.to_string();
  const std::string sanitized_pid = sanitize_pid_for_log(request.pid);
  LOG_DEBUG("submit recv reply_channel=%s pid=%s lang=%d",
            reply_channel_id.c_str(), sanitized_pid.c_str(),
            static_cast<int>(request.language));

  const auto submit_result =
      submission_request_handler_.handleSubmit(request, reply_channel_id);
  queue_immediate_response(slot, submit_result.response);
  for (const std::string &msg : submit_result.deferred_messages) {
    queue_protocol_response_for_channel(reply_channel_id, msg);
  }
}

void ClientSockets::handle_query_message(ConnectionSlot &slot,
                                         const QueryResultRequest &request) {
  queue_immediate_response(slot, query_request_handler_.handleQuery(request));
}

std::optional<ReplyChannel> ClientSockets::resolve_reply_channel_for_delivery(
    const std::string &reply_channel_id) const {
  const std::optional<ReplyChannel> reply_channel =
      ReplyChannel::parse(reply_channel_id);
  if (!reply_channel.has_value() ||
      reply_channel->slot_id >= static_cast<int>(connection_registry_.size())) {
    LOG_DEBUG("async drop invalid_channel reply_channel=%s",
              reply_channel_id.c_str());
    return std::nullopt;
  }

  return reply_channel;
}

/** @copydoc ClientSockets::onSubmissionStarted */
void ClientSockets::onSubmissionStarted(const SubmissionTask &task) {
  if (pool_ && pool_->is_stopping()) {
    return;
  }

  const std::optional<std::string> response =
      submission_event_responder_.onSubmissionStarted(task);
  if (!response.has_value()) {
    return;
  }

  queue_protocol_response_for_channel(task.reply_channel_id, *response);
}

/** @copydoc ClientSockets::onSubmissionFinished */
void ClientSockets::onSubmissionFinished(const SubmissionTask &task) {
  if (pool_ && pool_->is_stopping()) {
    return;
  }

  const std::optional<std::string> response =
      submission_event_responder_.onSubmissionFinished(task);
  if (!response.has_value()) {
    return;
  }

  queue_protocol_response_for_channel(task.reply_channel_id, *response);
}

/** @copydoc ClientSockets::queue_protocol_response_for_channel */
void ClientSockets::queue_protocol_response_for_channel(
    const std::string &reply_channel_id, std::string response) {
  std::string deferred_response = response;
  if (ack_barrier_.try_defer(reply_channel_id, std::move(deferred_response))) {
    return;
  }

  const std::optional<ReplyChannel> reply_channel =
      resolve_reply_channel_for_delivery(reply_channel_id);
  if (!reply_channel.has_value()) {
    return;
  }

  if (connection_registry_.slot(reply_channel->slot_id)
          .set_pending_response_if_session(reply_channel->session_id,
                                           std::move(response))) {
    wake_select_loop();
    return;
  }

  LOG_DEBUG("async drop stale_session reply_channel=%s",
            reply_channel_id.c_str());
}

/** @copydoc ClientSockets::wake_select_loop */
void ClientSockets::wake_select_loop() {
  if (wake_callback_) {
    wake_callback_();
  }
}
