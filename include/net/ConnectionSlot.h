#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

#include <netinet/in.h>
#include <unistd.h>

#include "Buffer.h"

enum class ConnectionSlotStatus {
  READABLE,
  WRITABLE,
  IDLE,
};

class ConnectionSlot {
public:
  ConnectionSlot() = default;

  int get_fd() const { return fd_; }
  uint64_t get_session_id() const { return session_id_; }

  void init(int fd, uint64_t session_id);
  void clear();

  bool is_readable();
  bool is_writable();
  void set_writable();
  void set_readable();
  void set_idle();

  bool read_message(int &read_size, std::string &message_body);
  int send(const std::string &result_data);

  void set_pending_response(std::string response);
  bool set_pending_response_if_session(uint64_t expected_session_id,
                                       std::string response);
  bool has_pending_response();
  std::string copy_pending_response();
  bool consume_pending_response(std::size_t bytes);

private:
  int fd_{0};
  uint64_t session_id_{0};
  std::mutex mutex_;
  Buffer input_buffer_;
  ConnectionSlotStatus status_{ConnectionSlotStatus::READABLE};
  std::string pending_response_;
  std::size_t pending_write_offset_{0};

  void queue_response_locked(std::string response);
};
