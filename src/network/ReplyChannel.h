#pragma once

#include <cstdint>
#include <optional>
#include <string>

/**
 * @brief 用于定位异步回推目标连接的逻辑通道。
 *
 * 一个 reply channel 由连接槽位和 session 组成，
 * 用来避免槽位复用后把旧 submission 的结果推给新连接。
 */
struct ReplyChannel {
  int slot_id;
  uint64_t session_id;

  /** @brief 序列化为 `slot:session` 文本。 */
  std::string to_string() const;
  /** @brief 从 `slot:session` 文本解析逻辑通道。 */
  static std::optional<ReplyChannel> parse(const std::string &value);
};
