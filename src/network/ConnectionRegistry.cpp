/**
 * @file ConnectionRegistry.cpp
 * @brief 连接槽位注册表与容器遍历逻辑实现。
 *
 * 负责固定大小 ConnectionSlot 容器的构造、槽位初始化/清理，
 * 以及将活跃连接按当前状态加入 select 所需的 fd_set。
 */
#include "network/ConnectionRegistry.h"

// ====================================================================
// ConnectionRegistry 实现
// ====================================================================

ConnectionRegistry::ConnectionRegistry(std::size_t slot_count) {
  slots_.reserve(slot_count);
  for (std::size_t i = 0; i < slot_count; ++i) {
    slots_.emplace_back(new ConnectionSlot);
    free_slot_ids_.push(i);
  }
}

void ConnectionRegistry::init_slot(std::size_t id, int fd,
                                   uint64_t session_id) {
  slots_.at(id)->init(fd, session_id);
}

int ConnectionRegistry::acquire_slot(int fd, uint64_t session_id) {
  if (free_slot_ids_.empty()) {
    return -1;
  }

  const std::size_t id = free_slot_ids_.front();
  free_slot_ids_.pop();
  init_slot(id, fd, session_id);
  return static_cast<int>(id);
}

void ConnectionRegistry::clear_slot(std::size_t id) { slots_.at(id)->clear(); }

void ConnectionRegistry::release_slot(std::size_t id) {
  if (slots_.at(id)->get_fd() == 0) {
    return;
  }

  clear_slot(id);
  free_slot_ids_.push(id);
}

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
