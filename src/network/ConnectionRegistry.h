#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <sys/select.h>

#include "network/ConnectionSlot.h"

class ConnectionRegistry {
public:
  explicit ConnectionRegistry(std::size_t slot_count = 0);

  std::size_t size() const { return slots_.size(); }

  ConnectionSlot &slot(std::size_t id) { return *slots_.at(id); }
  const ConnectionSlot &slot(std::size_t id) const { return *slots_.at(id); }

  void init_slot(std::size_t id, int fd, uint64_t session_id);
  void clear_slot(std::size_t id);
  int id_to_fd(std::size_t id) const;
  int add_to_sets(fd_set &read_sets, fd_set &write_sets) const;

private:
  std::vector<std::unique_ptr<ConnectionSlot>> slots_;
};
