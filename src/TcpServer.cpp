#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>

#include "TcpServer.h"

TcpServer::TcpServer(int port, AcceptCallback on_accept, ClientEventCallback on_client_event, 
                     ReNewSocketSets renew_socket_sets, bool nonblock, bool reuseaddr)
    : server_fd_(-1), running_(false), on_accept_(on_accept), on_client_event_(on_client_event), renew_socket_sets_(renew_socket_sets) {
    
    if (!create_and_bind_socket(port, nonblock, reuseaddr)) {
        throw std::runtime_error("Failed to create and bind server socket");
    }
}

TcpServer::~TcpServer() {
    stop();
    if (server_fd_ != -1) {
        close(server_fd_);
    }
}

bool TcpServer::create_and_bind_socket(int port, bool nonblock, bool reuseaddr) {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    if (reuseaddr) {
        int reuse = 1;
        if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            std::cerr << "setsockopt(SO_REUSEADDR) failed" << std::endl;
            close(server_fd_);
            server_fd_ = -1;
            return false;
        }
    }

    if (nonblock) {
        fcntl(server_fd_, F_SETFL, O_NONBLOCK);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Failed to bind socket" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    if (listen(server_fd_, 10) == -1) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(server_fd_);
        server_fd_ = -1;
        return false;
    }

    return true;
}

void TcpServer::start() {
    if (running_) {
        return;  // 已经在运行
    }
    
    running_ = true;
    fd_set read_fds, write_fds;
    int max_fd = server_fd_;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms

    while (running_) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(server_fd_, &read_fds);

        // 客户端 把相应的 socketfd 加入到对应的fd_set中
        if (renew_socket_sets_) {
            renew_socket_sets_(read_fds, write_fds);
        }

        // 计算最大fd
        max_fd = server_fd_;
        for (int fd = 0; fd <= FD_SETSIZE; ++fd) {
            if (FD_ISSET(fd, &read_fds) || FD_ISSET(fd, &write_fds)) {
                if (fd > max_fd) max_fd = fd;
            }
        }

        int ready = select(max_fd + 1, &read_fds, &write_fds, nullptr, &timeout);
        if (ready == -1) {
            std::cerr << "Select failed" << std::endl;
            break;
        }

        // 处理新连接
        if (FD_ISSET(server_fd_, &read_fds)) {
            int client_socket = accept(server_fd_, nullptr, nullptr);
            if (client_socket != -1) {
                // 设置为非阻塞
                fcntl(client_socket, F_SETFL, O_NONBLOCK);
                if (on_accept_) {
                    on_accept_(client_socket);
                }
            }
        }

        // 处理客户端事件
        if (on_client_event_) {
            on_client_event_(read_fds, write_fds);
        }
    }
}

void TcpServer::stop() {
    running_ = false;
}
