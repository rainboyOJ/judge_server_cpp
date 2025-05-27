//主程序，使用socket + select 进行IO复用
//judge server 连接的client应该是比较少的

#include <iostream>
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#include "Logger.h"
#include "client_sockets.h"

int main(int argc, char* argv[]) {

    // if( argc < 2 ) {
    //     std::cout << "Usage: " << argv[0] 
    //     <<  " <work_thread_num> <common_test_problem_num>"
    //     << " <problem_path>" << std::endl;
    //     return 1;
    // }

    //1. 创建testBox
    // TestBox test_box;
    std::unique_ptr<testBox> test_box = std::make_unique<testBox>(4,4,"/home/rainboy/mycode/boxtest/testData");
    //创建client_sockets
    ClientSockets client_sockets(test_box.get());

    // 创建服务器 socket 
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    // 设置 SO_REUSEADDR 选项
    int reuse = 1;  // 启用 SO_REUSEADDR 选项
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        LOG_ERROR("setsockopt(SO_REUSEADDR) failed");
        close(server_socket);
        return -1;
    }

    // 设置 socket 为非阻塞模式
    fcntl(server_socket, F_SETFL, O_NONBLOCK);

    // 绑定 socket 到本地地址
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    // server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(8000);
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Failed to bind socket" << std::endl;
        return 1;
    }

    // 监听连接请求
    if (listen(server_socket, 10) == -1) {
        std::cerr << "Failed to listen on socket" << std::endl;
        return 1;
    }
    else {
        std::cout << "Listening on port 8000" << std::endl;
    }

    // 主循环,监听 socket 事件并处理
    fd_set read_fds, write_fds;
    int max_fd = server_socket;
    // std::vector<int> client_sockets;

    struct timeval timeout; //设置等待时间
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;// 100ms
    while (true)
    {
        // 清空 read_fds 和 write_fds
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);

        // 将服务器 socket 添加到 read_fds
        FD_SET(server_socket, &read_fds);

        // 将客户端 socket 添加到 read_fds 和 write_fds,并得到client_sockets的最大fd
        max_fd = client_sockets.add_to_sets(read_fds,write_fds);
        if( max_fd < server_socket)
            max_fd = server_socket;

        // 使用 select 监听 socket 事件, 
        int ready = select(max_fd + 1, &read_fds, &write_fds, nullptr, &timeout);
        if (ready == -1) {
            std::cerr << "Select failed" << std::endl;
            return 1;
        }

        // 处理服务器 socket 上的事件
        if (FD_ISSET(server_socket, &read_fds)) {
            // 接受新的连接请求
            int client_socket = accept(server_socket, nullptr, nullptr);
            if (client_socket == -1) {
                std::cerr << "Failed to accept connection" << std::endl;
                continue;
            }

            // 设置客户端 socket 为非阻塞模式
            fcntl(client_socket, F_SETFL, O_NONBLOCK);

            // 将客户端 socket 添加到 client_sockets 列表中
            // 有可能添加不成功，这里会在client_sockets中处理
            client_sockets.add_socket(client_socket);
            std::cout << "Accepted connection" << std::endl;
        }
        // 处理客户端 socket 上的事件
        client_sockets.deal_events(read_fds, write_fds);
    }

    return 0;
}