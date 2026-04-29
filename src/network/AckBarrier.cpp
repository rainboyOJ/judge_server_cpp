#include "network/AckBarrier.h"

#include <utility>

void AckBarrier::mark_waiting(const std::string &reply_channel_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  awaiting_.insert(reply_channel_id);
}

std::vector<std::string>
AckBarrier::release(const std::string &reply_channel_id) {
  std::vector<std::string> deferred_messages;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    awaiting_.erase(reply_channel_id);
    const auto it = deferred_.find(reply_channel_id);
    if (it != deferred_.end()) {
      deferred_messages = std::move(it->second);
      deferred_.erase(it);
    }
  }
  return deferred_messages;
}

bool AckBarrier::try_defer(const std::string &reply_channel_id,
                           std::string response) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (awaiting_.count(reply_channel_id) == 0) {
    return false;
  }

  deferred_[reply_channel_id].push_back(std::move(response));
  return true;
}
