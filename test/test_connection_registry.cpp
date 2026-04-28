#include <arpa/inet.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "net/ConnectionRegistry.h"
#include "net/ConnectionSlot.h"

namespace {

void test_slot_init_and_clear() {
  int sockets[2] = {-1, -1};
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

  ConnectionSlot slot;
  slot.init(sockets[0], 42);

  assert(slot.get_fd() == sockets[0]);
  assert(slot.get_session_id() == 42);
  assert(slot.is_readable());
  assert(!slot.has_pending_response());

  slot.set_pending_response("ok");
  assert(slot.has_pending_response());

  slot.clear();

  assert(slot.get_fd() == 0);
  assert(slot.get_session_id() == 0);
  assert(slot.is_readable());
  assert(!slot.has_pending_response());

  close(sockets[1]);
}

void test_pending_response_is_session_safe() {
  int sockets[2] = {-1, -1};
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

  ConnectionSlot slot;
  slot.init(sockets[0], 7);

  assert(!slot.set_pending_response_if_session(6, "stale"));
  assert(!slot.has_pending_response());

  assert(slot.set_pending_response_if_session(7, "hello"));
  assert(slot.has_pending_response());
  assert(slot.is_writable());

  const std::string framed = slot.copy_pending_response();
  assert(framed.size() == sizeof(uint32_t) + 5);

  uint32_t body_len_net = 0;
  memcpy(&body_len_net, framed.data(), sizeof(body_len_net));
  assert(ntohl(body_len_net) == 5);
  assert(framed.substr(sizeof(uint32_t)) == "hello");

  assert(!slot.consume_pending_response(sizeof(uint32_t)));
  assert(slot.copy_pending_response() == "hello");
  assert(slot.has_pending_response());

  assert(slot.consume_pending_response(5));
  assert(!slot.has_pending_response());

  slot.clear();
  close(sockets[1]);
}

void test_registry_maps_logical_slot_id_to_fd() {
  int sockets[2] = {-1, -1};
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

  ConnectionRegistry registry(3);
  registry.init_slot(1, sockets[0], 99);

  assert(registry.id_to_fd(0) == 0);
  assert(registry.id_to_fd(1) == sockets[0]);
  assert(registry.slot(1).get_session_id() == 99);

  registry.clear_slot(1);
  assert(registry.id_to_fd(1) == 0);

  close(sockets[1]);
}

} // namespace

int main() {
  test_slot_init_and_clear();
  test_pending_response_is_session_safe();
  test_registry_maps_logical_slot_id_to_fd();
  return 0;
}
