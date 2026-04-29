#include <arpa/inet.h>

#include <atomic>
#include <cassert>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "network/ClientSockets.h"
#include "dispatch/JudgeWorkerPool.h"
#include "dispatch/SubmissionQueue.h"
#include "json.hpp"
#include "pipeline/JudgeCore.h"
#include "pipeline/ResultStore.h"
#include "pipeline/SubmissionService.h"
#include "runner/RunnerFactory.h"

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

class AsyncFlowHarness {
public:
  explicit AsyncFlowHarness(std::atomic<int> *wake_count = nullptr)
      : submission_service_(result_store_, runner_factory_, judge_core_),
        client_sockets_(4, submission_queue_, submission_service_,
                        [wake_count]() {
                          if (wake_count != nullptr) {
                            wake_count->fetch_add(1);
                          }
                        }),
        worker_pool_(1, submission_queue_, submission_service_, &client_sockets_) {
    createConnection();
  }

  void recreateConnection() { createConnection(); }

private:
  void createConnection() {
    int sockets[2] = {-1, -1};
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    client_fd_ = sockets[0];
    server_fd_ = sockets[1];
    client_sockets_.add_socket(server_fd_);
  }

public:
  ~AsyncFlowHarness() {
    if (client_fd_ >= 0) {
      close(client_fd_);
    }
    if (server_fd_ >= 0) {
      close(server_fd_);
    }
  }

  json roundTrip(const std::string &body) {
    write_framed_message(client_fd_, body);
    return waitForServerMessage();
  }

  json waitForServerMessage() {
    bool got_response = false;
    std::string response_body;

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

  void closeClient() {
    if (client_fd_ >= 0) {
      close(client_fd_);
      client_fd_ = -1;
    }
  }

  bool waitUntilSubmissionFinished(int submission_id) {
    for (int attempt = 0; attempt < kMaxPollAttempts; ++attempt) {
      pumpOnce();
      SubmissionResult result{};
      if (client_sockets_.submission_service().query(submission_id, result) &&
          (result.status == SubmissionStatus::FINISHED ||
           result.status == SubmissionStatus::FAILED)) {
        return true;
      }
    }
    return false;
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

  SubmissionQueue submission_queue_;
  ResultStore result_store_;
  RunnerFactory runner_factory_;
  JudgeCore judge_core_;
  SubmissionService submission_service_;
  ClientSockets client_sockets_;
  JudgeWorkerPool worker_pool_;
  int client_fd_{-1};
  int server_fd_{-1};
};

void test_query_result_returns_current_or_finished_snapshot_after_async_ack() {
  AsyncFlowHarness harness;
  const json ack = harness.roundTrip(
      R"({"type":"submit","uuid":95001,"pid":"1000","lang":2,"code":"a, b = map(int, input().split())\nprint(a + b)\n"})");

  assert(ack.at("type") == "submission_ack");
  assert(ack.at("status") == "QUEUED");
  const int submission_id = ack.at("submission_id").get<int>();

  const json queried = harness.roundTrip(
      std::string("{\"type\":\"query_result\",\"submission_id\":") +
      std::to_string(submission_id) + "}");

  assert(queried.at("submission_id") == submission_id);
  assert(queried.at("type").is_string());
  const std::string response_type = queried.at("type").get<std::string>();
  assert(response_type == "submission_update" ||
         response_type == "submission_finished");
}

void test_query_result_can_recover_final_snapshot_after_push_target_disconnects() {
  AsyncFlowHarness producer;
  const json ack = producer.roundTrip(
      R"({"type":"submit","uuid":95002,"pid":"1000","lang":2,"code":"a, b = map(int, input().split())\nprint(a + b)\n"})");

  assert(ack.at("type") == "submission_ack");
  const int submission_id = ack.at("submission_id").get<int>();

  producer.closeClient();
  assert(producer.waitUntilSubmissionFinished(submission_id));

  producer.recreateConnection();
  const json queried = producer.roundTrip(
      std::string("{\"type\":\"query_result\",\"submission_id\":") +
      std::to_string(submission_id) + "}");

  assert(queried.at("submission_id") == submission_id);
  assert(queried.at("type") == "submission_finished");
  assert(queried.at("status") == "FINISHED");
}

void test_async_notifier_wakes_select_loop_when_result_is_queued() {
  std::atomic<int> wake_count{0};
  AsyncFlowHarness harness(&wake_count);
  const json ack = harness.roundTrip(
      R"({"type":"submit","uuid":95003,"pid":"1000","lang":2,"code":"a, b = map(int, input().split())\nprint(a + b)\n"})");

  assert(ack.at("type") == "submission_ack");
  const int submission_id = ack.at("submission_id").get<int>();

  assert(harness.waitUntilSubmissionFinished(submission_id));
  assert(wake_count.load() > 0);
}

} // namespace

int main() {
  test_query_result_returns_current_or_finished_snapshot_after_async_ack();
  test_query_result_can_recover_final_snapshot_after_push_target_disconnects();
  test_async_notifier_wakes_select_loop_when_result_is_queued();
  return 0;
}
