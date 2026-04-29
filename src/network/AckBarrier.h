#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief 异步评测中 ack / finished 顺序保护器。
 *
 * 当 submission 入队但 ack 还未排进发送缓冲时，
 * 后台 worker 可能很快产生 finished/update 消息。
 * 本类将这些消息暂存到 deferred map 中，
 * 确保客户端先收到 submission_ack，再收到后续消息。
 */
class AckBarrier {
public:
  /**
   * @brief 标记该 channel 正在等待 ack 发出。
   */
  void mark_waiting(const std::string &reply_channel_id);

  /**
   * @brief 解除 ack 等待状态，并提取所有积压消息。
   * @return 该 channel 因等待 ack 而积压的所有协议消息。
   */
  std::vector<std::string> release(const std::string &reply_channel_id);

  /**
   * @brief 如果 channel 正处于等待 ack 状态，
   * 则将消息暂存。调用方应在确认未暂存后自行投递。
   * @return true 表示消息已被暂存（调用方本轮不应再投递）。
   * @return false 表示当前无需等待 ack，调用方应自行投递。
   */
  bool try_defer(const std::string &reply_channel_id,
                 std::string response);

private:
  std::mutex mutex_;
  std::unordered_set<std::string> awaiting_;
  std::unordered_map<std::string, std::vector<std::string>> deferred_;
};
