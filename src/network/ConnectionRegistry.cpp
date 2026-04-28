#include "network/ConnectionRegistry.h"

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <utility>

#include "common/Logger.h"

void ConnectionSlot::init(int fd, uint64_t session_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  fd_ = fd;
  session_id_ = session_id;
  status_ = ConnectionSlotStatus::READABLE;
  input_buffer_.retrieveAll();
  pending_response_.clear();
  pending_write_offset_ = 0;
}

void ConnectionSlot::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (fd_ != 0) {
    close(fd_);
  }
  fd_ = 0;
  session_id_ = 0;
  status_ = ConnectionSlotStatus::READABLE;
  input_buffer_.retrieveAll();
  pending_response_.clear();
  pending_write_offset_ = 0;
}

bool ConnectionSlot::is_readable() {
  std::lock_guard<std::mutex> lock(mutex_);
  return status_ == ConnectionSlotStatus::READABLE;
}

bool ConnectionSlot::is_writable() {
  std::lock_guard<std::mutex> lock(mutex_);
  return status_ == ConnectionSlotStatus::WRITABLE;
}

void ConnectionSlot::set_writable() {
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = ConnectionSlotStatus::WRITABLE;
}

void ConnectionSlot::set_readable() {
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = ConnectionSlotStatus::READABLE;
}

void ConnectionSlot::set_idle() {
  std::lock_guard<std::mutex> lock(mutex_);
  status_ = ConnectionSlotStatus::IDLE;
}

bool ConnectionSlot::read_message(int &tot_read, std::string &message_body) {
  tot_read = 0;
  message_body.clear();

  int saved_errno = 0;
  const ssize_t n = input_buffer_.readFd(fd_, &saved_errno);
  tot_read = static_cast<int>(n);

  if (n < 0) {
    if (saved_errno != EAGAIN && saved_errno != EWOULDBLOCK) {
      LOG_ERROR("ConnectionSlot::read error on socket %d, errno: %d", fd_,
                saved_errno);
      tot_read = -1;
    }
    return false;
  }
  if (n == 0) {
    LOG_INFO("Connection closed by peer on socket %d", fd_);
    return false;
  }

  if (input_buffer_.readableBytes() < sizeof(uint32_t)) {
    return false;
  }

  uint32_t msg_len_net = 0;
  memcpy(&msg_len_net, input_buffer_.peek(), sizeof(uint32_t));
  const uint32_t msg_len_host = ntohl(msg_len_net);

  LOG_DEBUG("Expected message body length = %u bytes\n", msg_len_host);

  if (input_buffer_.readableBytes() < sizeof(uint32_t) + msg_len_host) {
    LOG_DEBUG("Incomplete message: readableBytes()=%zu, needed=%zu",
              input_buffer_.readableBytes(), sizeof(uint32_t) + msg_len_host);
    return false;
  }

  input_buffer_.retrieve(sizeof(uint32_t));
  message_body = input_buffer_.retrieveAsString(msg_len_host);

#ifdef MUDEBUG
  LOG_DEBUG("Received message body: %s", message_body.c_str());
#endif

  set_idle();
  return true;
}

int ConnectionSlot::send(const std::string &result_data) {
  const int send_size = result_data.size();
  LOG_DEBUG("ready to write %d result_data bytes to socket %d", send_size,
            fd_);

  if (send_size <= 0) {
    return 0;
  }

  const ssize_t bytes = ::send(fd_, result_data.data(), result_data.size(), 0);
  if (bytes < 0) {
    return static_cast<int>(bytes);
  }
  return static_cast<int>(bytes);
}

void ConnectionSlot::set_pending_response(std::string response) {
  std::lock_guard<std::mutex> lock(mutex_);
  queue_response_locked(std::move(response));
}

bool ConnectionSlot::set_pending_response_if_session(
    uint64_t expected_session_id, std::string response) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (session_id_ != expected_session_id || fd_ == 0) {
    return false;
  }

  queue_response_locked(std::move(response));
  status_ = ConnectionSlotStatus::WRITABLE;
  return true;
}

bool ConnectionSlot::has_pending_response() {
  std::lock_guard<std::mutex> lock(mutex_);
  return !pending_response_.empty();
}

std::string ConnectionSlot::copy_pending_response() {
  std::lock_guard<std::mutex> lock(mutex_);
  return pending_response_.substr(pending_write_offset_);
}

bool ConnectionSlot::consume_pending_response(std::size_t bytes) {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_write_offset_ += bytes;
  if (pending_write_offset_ >= pending_response_.size()) {
    pending_response_.clear();
    pending_write_offset_ = 0;
    return true;
  }
  return false;
}

void ConnectionSlot::queue_response_locked(std::string response) {
  const uint32_t body_size = htonl(static_cast<uint32_t>(response.size()));
  pending_response_.append(reinterpret_cast<const char *>(&body_size),
                           sizeof(body_size));
  pending_response_ += response;
}

ConnectionRegistry::ConnectionRegistry(std::size_t slot_count) {
  slots_.reserve(slot_count);
  for (std::size_t i = 0; i < slot_count; ++i) {
    slots_.emplace_back(new ConnectionSlot);
  }
}

void ConnectionRegistry::init_slot(std::size_t id, int fd,
                                   uint64_t session_id) {
  slots_.at(id)->init(fd, session_id);
}

void ConnectionRegistry::clear_slot(std::size_t id) { slots_.at(id)->clear(); }

int ConnectionRegistry::id_to_fd(std::size_t id) const {
  return slots_.at(id)->get_fd();
}

int ConnectionRegistry::add_to_sets(fd_set &read_sets, fd_set &write_sets) const {
  int max_fd = 0;
  for (const auto &slot : slots_) {
    const int fd = slot->get_fd();
    if (fd == 0) {
      continue;
    }

    if (slot->is_readable()) {
      FD_SET(fd, &read_sets);
    } else if (slot->is_writable()) {
      FD_SET(fd, &write_sets);
    }

    if (fd > max_fd) {
      max_fd = fd;
    }
  }
  return max_fd;
}
