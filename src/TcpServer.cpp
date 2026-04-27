#include <sys/eventfd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#include <cerrno>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#include "TcpServer.h"

TcpServer::TcpServer(int port, AcceptCallback on_accept, ClientEventCallback on_client_event, 
                     ReNewSocketSets renew_socket_sets, bool nonblock, bool reuseaddr)
    : server_fd_(-1), wake_fd_(-1), running_(false), on_accept_(on_accept), on_client_event_(on_client_event), renew_socket_sets_(renew_socket_sets) {
    
    if (!create_wake_fd()) {
        throw std::runtime_error("Failed to create wake fd");
    }

    if (!create_and_bind_socket(port, nonblock, reuseaddr)) {
        throw std::runtime_error("Failed to create and bind server socket");
    }
}

TcpServer::~TcpServer() {
    stop();
    if (server_fd_ != -1) {
        close(server_fd_);
    }
    if (wake_fd_ != -1) {
        close(wake_fd_);
    }
}

bool TcpServer::create_wake_fd() {
    wake_fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (wake_fd_ == -1) {
        std::cerr << "Failed to create eventfd" << std::endl;
        return false;
    }
    return true;
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

    while (running_) {
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_SET(server_fd_, &read_fds);
        FD_SET(wake_fd_, &read_fds);

        // 客户端 把相应的 socketfd 加入到对应的fd_set中
        if (renew_socket_sets_) {
            renew_socket_sets_(read_fds, write_fds);
        }

        // 计算最大fd
        int max_fd = (server_fd_ > wake_fd_) ? server_fd_ : wake_fd_;
        for (int fd = 0; fd < FD_SETSIZE; ++fd) {
            if (FD_ISSET(fd, &read_fds) || FD_ISSET(fd, &write_fds)) {
                if (fd > max_fd) max_fd = fd;
            }
        }

        // 后台线程可能会把某个连接切到 WRITABLE；eventfd 负责把阻塞中的
        // select 立即唤醒，这样循环可以安全地使用阻塞等待而不是超时轮询。
        int ready = select(max_fd + 1, &read_fds, &write_fds, nullptr, nullptr);
        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Select failed" << std::endl;
            break;
        }

        if (FD_ISSET(wake_fd_, &read_fds)) {
            drain_wake_fd();
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

void TcpServer::wake() {
    if (wake_fd_ == -1) {
        return;
    }

    const uint64_t one = 1;
    const ssize_t written = write(wake_fd_, &one, sizeof(one));
    if (written < 0 && errno != EAGAIN) {
        std::cerr << "Failed to write wake event" << std::endl;
    }
}

void TcpServer::drain_wake_fd() {
    uint64_t value = 0;
    ssize_t bytes = 0;
    while ((bytes = read(wake_fd_, &value, sizeof(value))) > 0) {
    }

    if (bytes < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        std::cerr << "Failed to drain wake event" << std::endl;
    }
}

void TcpServer::stop() {
    running_ = false;
    // stop() 可能由其他线程调用；如果主线程此时正阻塞在 select(nullptr)
    // 中，必须显式写 eventfd 把它唤醒，否则事件循环无法及时退出。
    wake();
}
