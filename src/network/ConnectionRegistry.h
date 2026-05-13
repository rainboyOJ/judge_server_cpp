/**
 * @file ConnectionRegistry.h
 * @brief 连接槽位容器的管理接口。
 *
 * 它是 ConnectionSlot 的集合管理者，负责：
 * - 预分配固定数量的连接槽位
 * - 按索引获取/初始化/清理具体槽位
 * - 把当前所有活跃连接按照读写状态加入 fd_set
 *
 * 当前实现是一个固定大小的槽位数组，用来承载所有活跃连接。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

#include <sys/select.h>

#include "network/ConnectionSlot.h"

/**
 * @brief 连接槽位容器。
 *
 * 它是 ClientSockets 和底层 socket 连接之间的中间层：
 * - 槽位预分配：构造时创建固定数量的 ConnectionSlot。
 * - 生命周期：init_slot / clear_slot 对应新连接接入 / 断连。
 * - select 参与：populate_socket_sets 决定哪些 fd 应该进入本轮的 read_fd_set 和 write_fd_set。
 */
class ConnectionRegistry {
public:
  /**
   * @brief 预分配指定数量的连接槽位。
   * @param slot_count 最大槽位数量。
   */
  explicit ConnectionRegistry(std::size_t slot_count = 0);

  /** @brief 当前槽位总数。 */
  std::size_t size() const { return slots_.size(); }

  /**
   * @brief 获取指定索引的槽位引用。
   *
   * 调用方可以用它来读写连接状态、设置待发送响应等。
   */
  ConnectionSlot &slot(std::size_t id) { return *slots_.at(id); }
  const ConnectionSlot &slot(std::size_t id) const { return *slots_.at(id); }

  /** @brief 用新 fd 和 session_id 初始化指定槽位。 */
  void init_slot(std::size_t id, int fd, uint64_t session_id);

  /** @brief 分配一个空闲逻辑槽位并初始化，失败返回 -1。 */
  int acquire_slot(int fd, uint64_t session_id);

  /** @brief 清空指定槽位（关闭 fd 并重置状态）。 */
  void clear_slot(std::size_t id);

  /** @brief 释放一个已占用槽位，使其重新进入空闲队列。 */
  void release_slot(std::size_t id);

  /** @brief 通过槽位 id 获取当前 fd，0 表示空闲。 */
  int id_to_fd(std::size_t id) const;

  /**
   * @brief 把当前所有活跃连接加入到 read/write fd_set。
   * @return int 最大的 fd 值，供 select 使用。
   *
   * 遍历所有槽位：
   * - READABLE 的槽位加入 read_fds
   * - WRITABLE 的槽位加入 write_fds
   * - 跳过 fd == 0 的空槽位
   */
  int populate_socket_sets(fd_set &read_sets, fd_set &write_sets) const;

private:
  std::vector<std::unique_ptr<ConnectionSlot>> slots_;
  std::queue<std::size_t> free_slot_ids_;
};
