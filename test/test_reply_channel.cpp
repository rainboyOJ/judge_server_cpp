#include <cassert>
#include <cstdint>
#include <optional>
#include <string>

#include "network/ReplyChannel.h"

namespace {

void test_format_round_trip() {
  const ReplyChannel channel{3, 42};

  const std::string formatted = channel.to_string();
  assert(formatted == "3:42");

  const std::optional<ReplyChannel> parsed = ReplyChannel::parse(formatted);
  assert(parsed.has_value());
  assert(parsed->slot_id == 3);
  assert(parsed->session_id == 42);
}

void test_parse_rejects_invalid_inputs() {
  assert(!ReplyChannel::parse("342").has_value());
  assert(!ReplyChannel::parse("-1:42").has_value());
  assert(!ReplyChannel::parse("3:0").has_value());
  assert(!ReplyChannel::parse("x:42").has_value());
  assert(!ReplyChannel::parse("3:y").has_value());
}

} // namespace

int main() {
  test_format_round_trip();
  test_parse_rejects_invalid_inputs();
  return 0;
}
