#include <cassert>
#include <arpa/inet.h>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "json.hpp"
#include "network/ClientSockets.h"
#include "pipeline/SubmissionVerdictReducer.h"
#include "pipeline/ResultStore.h"
#include "pipeline/SubmissionService.h"
#include "runner/RunnerFactory.h"

namespace {

using json = nlohmann::json;

bool read_exact(int fd, void *buffer, std::size_t size) {
  char *out = static_cast<char *>(buffer);
  std::size_t read_size = 0;
  while (read_size < size) {
    const ssize_t bytes = recv(fd, out + read_size, size - read_size, 0);
    if (bytes <= 0) {
      return false;
    }
    read_size += static_cast<std::size_t>(bytes);
  }
  return true;
}

std::string read_framed_body(int fd) {
  uint32_t body_len_net = 0;
  assert(read_exact(fd, &body_len_net, sizeof(body_len_net)));

  const uint32_t body_len = ntohl(body_len_net);
  std::string body(body_len, '\0');
  assert(read_exact(fd, body.data(), body.size()));
  return body;
}

void test_client_sockets_accepts_injected_submission_service() {
  ResultStore result_store;
  RunnerFactory runner_factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService submission_service(result_store, runner_factory,
                                       judge_verdict_reducer);

  ClientSockets client_sockets(16, submission_service);

  assert(&client_sockets.submission_service() == &submission_service);
}

void test_client_sockets_reports_connection_limit_before_close() {
  ResultStore result_store;
  RunnerFactory runner_factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService submission_service(result_store, runner_factory,
                                       judge_verdict_reducer);
  ClientSockets client_sockets(0, submission_service);

  int sockets[2] = {-1, -1};
  assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

  client_sockets.register_client_socket(sockets[0]);

  const json response = json::parse(read_framed_body(sockets[1]));
  assert(response.at("status").get<std::string>() == "FAILED");
  assert(response.at("verdict").get<std::string>() == "SYSTEM_ERROR");
  assert(response.at("message").get<std::string>() ==
         "judge server is busy, too many active connections");

  char extra = '\0';
  assert(recv(sockets[1], &extra, sizeof(extra), 0) == 0);
  close(sockets[1]);
}

} // namespace

int main() {
  test_client_sockets_accepts_injected_submission_service();
  test_client_sockets_reports_connection_limit_before_close();
  return 0;
}
