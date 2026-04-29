#include "network/ReplyChannel.h"

#include <exception>

std::string ReplyChannel::to_string() const {
  return std::to_string(slot_id) + ":" + std::to_string(session_id);
}

std::optional<ReplyChannel> ReplyChannel::parse(const std::string &value) {
  const std::size_t separator = value.find(':');
  if (separator == std::string::npos || separator == 0 ||
      separator + 1 >= value.size() || value.find(':', separator + 1) != std::string::npos) {
    return std::nullopt;
  }

  try {
    std::size_t slot_pos = 0;
    std::size_t session_pos = 0;
    const int slot_id = std::stoi(value.substr(0, separator), &slot_pos);
    const uint64_t session_id =
        std::stoull(value.substr(separator + 1), &session_pos);
    if (slot_pos != separator || session_pos != value.size() - separator - 1 ||
        slot_id < 0 || session_id == 0) {
      return std::nullopt;
    }
    return ReplyChannel{slot_id, session_id};
  } catch (const std::exception &) {
    return std::nullopt;
  }
}
