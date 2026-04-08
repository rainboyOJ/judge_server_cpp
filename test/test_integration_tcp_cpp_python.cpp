#include <arpa/inet.h>

#include <cassert>
#include <cstring>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "client_sockets.h"
#include "json.hpp"

using json = nlohmann::json;

namespace {

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

json submit_and_receive_json(const std::string &request_body) {
    int sockets[2] = {-1, -1};
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);

    testBox box(1, 4, std::string(PROJECT_ROOT_DIR) + "/testData");
    ClientSockets client_sockets(&box);
    client_sockets.add_socket(sockets[1]);

    write_framed_message(sockets[0], request_body);

    bool got_response = false;
    std::string response_body;
    for (int attempt = 0; attempt < 400 && !got_response; ++attempt) {
        fd_set read_sets;
        fd_set write_sets;
        FD_ZERO(&read_sets);
        FD_ZERO(&write_sets);

        const int max_fd = client_sockets.add_to_sets(read_sets, write_sets);
        timeval timeout{};
        timeout.tv_usec = 10000;

        const int ready =
            select(max_fd + 1, &read_sets, &write_sets, nullptr, &timeout);
        assert(ready >= 0);

        if (ready > 0) {
            client_sockets.deal_events(read_sets, write_sets);
        }

        fd_set client_read_set;
        FD_ZERO(&client_read_set);
        FD_SET(sockets[0], &client_read_set);
        timeval client_timeout{};
        const int client_ready = select(sockets[0] + 1, &client_read_set,
                                        nullptr, nullptr, &client_timeout);
        assert(client_ready >= 0);
        if (client_ready > 0 && FD_ISSET(sockets[0], &client_read_set)) {
            response_body = read_exact_message(sockets[0]);
            got_response = true;
        }
    }

    close(sockets[0]);
    close(sockets[1]);

    assert(got_response);
    return json::parse(response_body);
}

void test_tcp_json_submission_flows_through_service_and_returns_protocol_shape() {
    const json response = submit_and_receive_json(
        R"({"uuid":94001,"pid":"1000","lang":2,"code":"a, b = map(int, input().split())\nprint(a + b)\n"})");

    assert(response.contains("submission_id"));
    assert(response.at("submission_id").is_number_integer());
    assert(response.at("submission_id").get<int>() > 0);
    assert(response.at("status") == "FINISHED");
    assert(response.at("verdict") == "AC");
    assert(response.at("code") == 0);
    assert(response.at("msg").is_string());
    assert(response.at("case_results").is_array());
    assert(!response.at("case_results").empty());
}

void test_tcp_cpp_submission_also_flows_through_service_and_returns_ac() {
    const json response = submit_and_receive_json(
        R"({"uuid":94002,"pid":"1000","lang":0,"code":"#include <iostream>\nint main(){long long a,b;std::cin>>a>>b;std::cout<<(a+b)<<'\\n';return 0;}\n"})");

    assert(response.contains("submission_id"));
    assert(response.at("submission_id").is_number_integer());
    assert(response.at("submission_id").get<int>() > 0);
    assert(response.at("status") == "FINISHED");
    assert(response.at("verdict") == "AC");
    assert(response.at("code") == 0);
    assert(response.at("case_results").is_array());
    assert(!response.at("case_results").empty());
}

} // namespace

int main() {
    test_tcp_json_submission_flows_through_service_and_returns_protocol_shape();
    test_tcp_cpp_submission_also_flows_through_service_and_returns_ac();
    return 0;
}
