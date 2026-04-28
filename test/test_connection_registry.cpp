#include <arpa/inet.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "network/ConnectionRegistry.h"
#include "network/ConnectionSlot.h"

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

void test_registry_acquires_and_releases_slots() {
  int sockets_a[2] = {-1, -1};
  int sockets_b[2] = {-1, -1};
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets_a) == 0);
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets_b) == 0);

  ConnectionRegistry registry(2);

  const int slot_a = registry.acquire_slot(sockets_a[0], 101);
  const int slot_b = registry.acquire_slot(sockets_b[0], 102);
  const int slot_full = registry.acquire_slot(123, 103);

  assert(slot_a == 0);
  assert(slot_b == 1);
  assert(slot_full == -1);
  assert(registry.slot(slot_a).get_session_id() == 101);
  assert(registry.slot(slot_b).get_session_id() == 102);

  registry.release_slot(static_cast<std::size_t>(slot_a));
  assert(registry.id_to_fd(slot_a) == 0);

  const int slot_reused = registry.acquire_slot(sockets_a[0], 201);
  assert(slot_reused == slot_a);
  assert(registry.slot(slot_reused).get_session_id() == 201);

  registry.release_slot(static_cast<std::size_t>(slot_reused));
  registry.release_slot(static_cast<std::size_t>(slot_b));
  close(sockets_a[1]);
  close(sockets_b[1]);
}

} // namespace

int main() {
  test_slot_init_and_clear();
  test_pending_response_is_session_safe();
  test_registry_maps_logical_slot_id_to_fd();
  test_registry_acquires_and_releases_slots();
  return 0;
}
