#include <cassert>
#include <chrono>
#include <future>
#include <thread>

#include "network/TcpServer.h"

namespace {

void test_stop_alone_unblocks_blocking_select_loop() {
  TcpServer server(0, nullptr, nullptr, nullptr);

  auto started = std::make_shared<std::promise<void>>();
  std::shared_future<void> started_future(started->get_future());

  auto loop_future = std::async(std::launch::async, [&server, started]() {
    started->set_value();
    server.start();
  });

  assert(started_future.wait_for(std::chrono::seconds(1)) ==
         std::future_status::ready);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  server.stop();

  const auto status = loop_future.wait_for(std::chrono::milliseconds(500));
  assert(status == std::future_status::ready);
}

} // namespace

int main() {
  test_stop_alone_unblocks_blocking_select_loop();
  return 0;
}
