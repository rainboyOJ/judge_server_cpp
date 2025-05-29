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
#include "TcpServer.h"

int main(int argc, char* argv[]) {

    // if( argc < 2 ) {
    //     std::cout << "Usage: " << argv[0] 
    //     <<  " <work_thread_num> <common_test_problem_num>"
    //     << " <problem_path>" << std::endl;
    //     return 1;
    // }

    //1. 创建testBox
    // TestBox test_box;
    std::unique_ptr<testBox> test_box = std::make_unique<testBox>(4,4,"/home/rainboy/mycode/RainboyOJ/boxtest/testData");
    ClientSockets client_sockets(test_box.get());

    try {
        // 创建TCP服务器，监听8000端口
        TcpServer server(8000, 
            // 接受新连接的回调
            [&client_sockets](int client_fd) {
                client_sockets.add_socket(client_fd);
                LOG_INFO("Accepted connection, fd: %d", client_fd);
            },
            // 处理客户端事件的回调
            [&client_sockets](fd_set& read_fds, fd_set& write_fds) {
                client_sockets.deal_events(read_fds, write_fds);
            },
            // 加入socket到fd_set中
            [&client_sockets](fd_set& read_fds, fd_set& write_fds) {
                client_sockets.add_to_sets(read_fds, write_fds);
            }
        );
        
        std::cout << "Listening on port 8000" << std::endl;
        std::cout << "Server socket fd: " << server.get_server_fd() << std::endl;
        
        // 启动服务器事件循环
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "服务器启动失败: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}