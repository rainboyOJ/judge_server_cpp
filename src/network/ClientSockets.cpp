/**
 * @file client_sockets.cpp
 * @brief TCP 连接管理、协议接入与异步结果回推实现。
 */

#include <sys/select.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <exception>
#include <memory>

#include "network/ClientSockets.h"
#include "common/Logger.h"
#include "protocol/JudgeProtocol.h"

namespace {

/**
 * @brief 判断快照是否已经进入终态。
 */
bool is_terminal_status(SubmissionStatus status) {
  return status == SubmissionStatus::FINISHED ||
         status == SubmissionStatus::FAILED;
}

} // namespace

/** @copydoc ClientSockets::ClientSockets */
ClientSockets::ClientSockets(std::size_t slot_count,
                             SubmissionQueue &submission_queue,
                             WakeCallback wake_callback)
    : submission_queue_(submission_queue), connection_registry_(slot_count),
      submission_service_(result_store_, runner_factory_, judge_core_),
      wake_callback_(std::move(wake_callback)) {}

/** @copydoc ClientSockets::add_to_sets */
int ClientSockets::add_to_sets(fd_set &read_sets, fd_set &write_sets) {
  return connection_registry_.add_to_sets(read_sets, write_sets);
}

/** @copydoc ClientSockets::add_socket */
void ClientSockets::add_socket(int client_socket) {
  const int slot_id =
      connection_registry_.acquire_slot(client_socket, next_session_id_++);
  if (slot_id < 0) {
    LOG_DEBUG("connection registry is FULL, disconnect socket %d", client_socket);
    // TODO 发送评测已经满的信息
    //  并关闭连接
    close(client_socket);
  } else {
    LOG_DEBUG("add socket %d to logical slot %d", client_socket, slot_id);
  }
}

/** @copydoc ClientSockets::del_socket */
void ClientSockets::del_socket(int slot_id) {
  connection_registry_.release_slot(static_cast<std::size_t>(slot_id));
}

/** @copydoc ClientSockets::deal_events */
void ClientSockets::deal_events(const fd_set &read_sets,
                                const fd_set &write_sets) {

  for (std::size_t i = 0; i < connection_registry_.size(); i++) {
    ConnectionSlot &slot = connection_registry_.slot(i);
    int client_socket = slot.get_fd();
    if (client_socket == 0)
      continue;
    int slot_id = static_cast<int>(i);
    // 读取事件
    if (FD_ISSET(client_socket, &read_sets)) {
      LOG_DEBUG("read event on socket %d\n", client_socket);
      int bytes_read = 0;
      std::string message_body;
      const bool has_complete_message = slot.read_message(bytes_read, message_body);
      if (bytes_read == 0) {
        LOG_INFO("Connection closed : %d\n", client_socket);
        del_socket(slot_id);
      } else if (bytes_read < 0) {
        LOG_ERROR("Failed to read frome socket: %d\n", client_socket);
        del_socket(slot_id);
      } else if (has_complete_message) { // 没有发生错误
        SubmissionRequest request{};
        if (protocol_.decodeRequest(message_body, request)) {
          const int submission_id =
              submission_service_.createSubmission(request);
          if (submission_id <= 0) {
            slot.set_pending_response(
                protocol_.encodeError("failed to create submission"));
            slot.set_writable();
            continue;
          }

          SubmissionTask task{};
          task.submission_id = submission_id;
          task.request = request;
          task.reply_channel_id =
              make_reply_channel_id(slot_id, slot.get_session_id());

          mark_channel_waiting_for_ack(task.reply_channel_id);

          if (!submission_queue_.push(task)) {
            mark_channel_ack_sent(task.reply_channel_id);
            slot.set_pending_response(
                protocol_.encodeError("submission queue unavailable"));
            slot.set_writable();
          } else {
            slot.set_pending_response(protocol_.encodeSubmissionAck(submission_id));
            slot.set_writable();
            mark_channel_ack_sent(task.reply_channel_id);
          }
        } else {
          QueryResultRequest query_request{};
          if (protocol_.decodeQueryRequest(message_body, query_request)) {
            SubmissionResult result{};
            if (!submission_service_.query(query_request.submission_id,
                                           result)) {
              slot.set_pending_response(
                  protocol_.encodeError("submission not found"));
            } else {
              slot.set_pending_response(protocol_.encodeResult(result));
            }
            slot.set_writable();
          } else {
            slot.set_pending_response(protocol_.encodeError("bad request"));
            slot.set_writable();
          }
        }
      } // 没有读取发生错误else end
      else {
        LOG_DEBUG("read bytes_read = %d, but not get All testProblem data",
                  bytes_read);
      }
    }
    // 写事件
    else if (FD_ISSET(client_socket, &write_sets)) {
      LOG_DEBUG("write event on socket %d\n", client_socket);
      // 1. 检查socket状态，确保可写
      if (!slot.is_writable()) // 不是可写状态
        continue;

      if (!slot.has_pending_response()) {
        slot.set_readable();
        continue;
      }
      std::string result_data = slot.copy_pending_response();
#ifdef MUDEBUG
      // LOG_DEBUG("send %d bytes to socket %d", result_data.size(),
      // client_socket); debug_print_uint8_t_vector(result_data);
#endif
      int bytes_write = slot.send(result_data);
      if (bytes_write < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          continue;
        }
        LOG_ERROR("Failed to send response to socket %d", client_socket);
        del_socket(slot_id);
        continue;
      }

      if (!slot.consume_pending_response(bytes_write)) {
        continue;
      }

      slot.set_readable(); // 转入readable 的状态
    }
  }
}

/** @copydoc ClientSockets::onSubmissionStarted */
void ClientSockets::onSubmissionStarted(const SubmissionTask &task) {
  SubmissionResult result{};
  if (!submission_service_.query(task.submission_id, result)) {
    return;
  }

  if (result.status == SubmissionStatus::QUEUED) {
    return;
  }

  queue_protocol_response_for_channel(task.reply_channel_id,
                                      protocol_.encodeSubmissionUpdate(result));
}

/** @copydoc ClientSockets::onSubmissionFinished */
void ClientSockets::onSubmissionFinished(const SubmissionTask &task) {
  SubmissionResult result{};
  if (!submission_service_.query(task.submission_id, result)) {
    return;
  }

  const std::string response = is_terminal_status(result.status)
                                   ? protocol_.encodeSubmissionFinished(result)
                                   : protocol_.encodeSubmissionUpdate(result);
  queue_protocol_response_for_channel(task.reply_channel_id, response);
}

/** @copydoc ClientSockets::make_reply_channel_id */
std::string ClientSockets::make_reply_channel_id(int slot_id,
                                                 uint64_t session_id) const {
  return std::to_string(slot_id) + ":" + std::to_string(session_id);
}

/** @copydoc ClientSockets::parse_reply_channel_id */
bool ClientSockets::parse_reply_channel_id(const std::string &reply_channel_id,
                                           int &slot_id,
                                           uint64_t &session_id) const {
  const std::size_t separator = reply_channel_id.find(':');
  if (separator == std::string::npos) {
    return false;
  }

  try {
    slot_id = std::stoi(reply_channel_id.substr(0, separator));
    session_id = static_cast<uint64_t>(
        std::stoull(reply_channel_id.substr(separator + 1)));
  } catch (const std::exception &) {
    return false;
  }

  return slot_id >= 0 &&
         slot_id < static_cast<int>(connection_registry_.size()) &&
         session_id > 0;
}

/** @copydoc ClientSockets::queue_protocol_response_for_channel */
void ClientSockets::queue_protocol_response_for_channel(
    const std::string &reply_channel_id, std::string response) {
  {
    std::lock_guard<std::mutex> lock(notifier_mutex_);
    if (awaiting_ack_channels_.count(reply_channel_id) > 0) {
      deferred_protocol_messages_[reply_channel_id].push_back(
          std::move(response));
      return;
    }
  }

  int slot_id = -1;
  uint64_t session_id = 0;
  if (!parse_reply_channel_id(reply_channel_id, slot_id, session_id)) {
    return;
  }

  if (connection_registry_.slot(slot_id)
          .set_pending_response_if_session(session_id, std::move(response))) {
    wake_select_loop();
  }
}

/** @copydoc ClientSockets::mark_channel_waiting_for_ack */
void ClientSockets::mark_channel_waiting_for_ack(
    const std::string &reply_channel_id) {
  std::lock_guard<std::mutex> lock(notifier_mutex_);
  awaiting_ack_channels_.insert(reply_channel_id);
}

/** @copydoc ClientSockets::mark_channel_ack_sent */
void ClientSockets::mark_channel_ack_sent(const std::string &reply_channel_id) {
  std::vector<std::string> deferred_messages;
  {
    std::lock_guard<std::mutex> lock(notifier_mutex_);
    awaiting_ack_channels_.erase(reply_channel_id);
    const auto it = deferred_protocol_messages_.find(reply_channel_id);
    if (it != deferred_protocol_messages_.end()) {
      deferred_messages = std::move(it->second);
      deferred_protocol_messages_.erase(it);
    }
  }

  for (std::string &message : deferred_messages) {
    queue_protocol_response_for_channel(reply_channel_id, std::move(message));
  }
}

/** @copydoc ClientSockets::wake_select_loop */
void ClientSockets::wake_select_loop() {
  if (wake_callback_) {
    wake_callback_();
  }
}
