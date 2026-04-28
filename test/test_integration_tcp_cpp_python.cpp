#include <arpa/inet.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "network/ClientSockets.h"
#include "dispatch/JudgeWorkerPool.h"
#include "dispatch/SubmissionQueue.h"
#include "json.hpp"

using json = nlohmann::json;

namespace {

constexpr int kPollTimeoutUsec = 10000;
constexpr int kMaxPollAttempts = 400;

std::string read_exact_message(int fd) {
  uint32_t body_len_net = 0;
  const ssize_t prefix_read =
      recv(fd, &body_len_net, sizeof(body_len_net), MSG_WAITALL);
  assert(prefix_read == static_cast<ssize_t>(sizeof(body_len_net)));

  const uint32_t body_len = ntohl(body_len_net);
  std::string body(body_len, '\0');
  const ssize_t body_read = recv(fd, body.data(), body.size(), MSG_WAITALL);
  assert(body_read == static_cast<ssize_t>(body.size()));
  return body;
}

void write_framed_message(int fd, const std::string &body) {
  const uint32_t body_len_net = htonl(static_cast<uint32_t>(body.size()));
  const ssize_t prefix_written =
      send(fd, &body_len_net, sizeof(body_len_net), 0);
  assert(prefix_written == static_cast<ssize_t>(sizeof(body_len_net)));

  const ssize_t body_written = send(fd, body.data(), body.size(), 0);
  assert(body_written == static_cast<ssize_t>(body.size()));
}

class SocketHarness {
public:
  SocketHarness()
      : box_(1, 4, std::string(PROJECT_ROOT_DIR) + "/testData"),
        client_sockets_(&box_, submission_queue_),
        worker_pool_(1, submission_queue_, client_sockets_.submission_service(),
                     &client_sockets_) {
    int sockets[2] = {-1, -1};
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    client_fd_ = sockets[0];
    server_fd_ = sockets[1];
    client_sockets_.add_socket(server_fd_);
  }

  ~SocketHarness() {
    if (client_fd_ >= 0) {
      close(client_fd_);
    }
    if (server_fd_ >= 0) {
      close(server_fd_);
    }
  }

  void sendRequest(const std::string &body) {
    write_framed_message(client_fd_, body);
  }

  json waitForServerMessage() {
    std::string response_body;
    bool got_response = false;
    for (int attempt = 0; attempt < kMaxPollAttempts && !got_response;
         ++attempt) {
      pumpOnce();
      fd_set client_read_set;
      FD_ZERO(&client_read_set);
      FD_SET(client_fd_, &client_read_set);
      timeval client_timeout{};
      const int client_ready = select(client_fd_ + 1, &client_read_set, nullptr,
                                      nullptr, &client_timeout);
      assert(client_ready >= 0);
      if (client_ready > 0 && FD_ISSET(client_fd_, &client_read_set)) {
        response_body = read_exact_message(client_fd_);
        got_response = true;
      }
    }

    assert(got_response);
    return json::parse(response_body);
  }

private:
  void pumpOnce() {
    fd_set read_sets;
    fd_set write_sets;
    FD_ZERO(&read_sets);
    FD_ZERO(&write_sets);

    const int max_fd = client_sockets_.add_to_sets(read_sets, write_sets);
    timeval timeout{};
    timeout.tv_usec = kPollTimeoutUsec;

    const int ready =
        select(max_fd + 1, &read_sets, &write_sets, nullptr, &timeout);
    assert(ready >= 0);
    if (ready > 0) {
      client_sockets_.deal_events(read_sets, write_sets);
    }
  }

  testBox box_;
  SubmissionQueue submission_queue_;
  ClientSockets client_sockets_;
  JudgeWorkerPool worker_pool_;
  int client_fd_{-1};
  int server_fd_{-1};
};

void assert_submit_ack(const json &response) {
  assert(response.at("type") == "submission_ack");
  assert(response.contains("submission_id"));
  assert(response.at("submission_id").is_number_integer());
  assert(response.at("submission_id").get<int>() > 0);
  assert(response.at("status") == "QUEUED");
  assert(response.at("verdict") == "PENDING");
}

void assert_finished_ac(const json &response) {
  assert(response.at("type") == "submission_finished");
  assert(response.contains("submission_id"));
  assert(response.at("submission_id").is_number_integer());
  assert(response.at("submission_id").get<int>() > 0);
  assert(response.at("status") == "FINISHED");
  assert(response.at("verdict") == "AC");
  assert(response.at("case_results").is_array());
  assert(!response.at("case_results").empty());
}

void test_tcp_python_submission_ack_is_immediate_and_finish_is_pushed_later() {
  SocketHarness harness;
  harness.sendRequest(
      R"({"type":"submit","uuid":94001,"pid":"1000","lang":2,"code":"a, b = map(int, input().split())\nprint(a + b)\n"})");

  const json ack = harness.waitForServerMessage();
  assert_submit_ack(ack);

  const json finished = harness.waitForServerMessage();
  assert_finished_ac(finished);
  assert(finished.at("submission_id") == ack.at("submission_id"));
}

void test_tcp_cpp_submission_ack_is_immediate_and_finish_is_pushed_later() {
  SocketHarness harness;
  harness.sendRequest(
      R"({"type":"submit","uuid":94002,"pid":"1000","lang":0,"code":"#include <iostream>\nint main(){long long a,b;std::cin>>a>>b;std::cout<<(a+b)<<'\\n';return 0;}\n"})");

  const json ack = harness.waitForServerMessage();
  assert_submit_ack(ack);

  const json finished = harness.waitForServerMessage();
  assert_finished_ac(finished);
  assert(finished.at("submission_id") == ack.at("submission_id"));
}

} // namespace

int main() {
  test_tcp_python_submission_ack_is_immediate_and_finish_is_pushed_later();
  test_tcp_cpp_submission_ack_is_immediate_and_finish_is_pushed_later();
  return 0;
}
