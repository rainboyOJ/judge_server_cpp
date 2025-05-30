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
#include "common/Config.h"

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string config_file = "config/config.json"; // 默认配置文件路径
    
    if (argc >= 2) {
        config_file = argv[1];
    }
    
    // 加载配置
    Config& config = Config::getInstance();
    config.loadFromFile(config_file);
    
    std::cout << "=== BoxTest Server Starting ===" << std::endl;
    std::cout << "Config file: " << config_file << std::endl;
    std::cout << "Server port: " << config.getServerPort() << std::endl;
    std::cout << "Worker threads: " << config.getWorkerThreadCount() << std::endl;
    std::cout << "Max concurrent tests: " << config.getMaxConcurrentTests() << std::endl;
    std::cout << "Test data path: " << config.getTestDataPath() << std::endl;
    std::cout << "================================" << std::endl;
    
    //1. 创建testBox，使用配置中的参数
    std::unique_ptr<testBox> test_box = std::make_unique<testBox>(
        config.getWorkerThreadCount(),
        config.getMaxConcurrentTests(),
        config.getTestDataPath()
    );
    ClientSockets client_sockets(test_box.get());

    try {
        // 创建TCP服务器，使用配置中的端口
        TcpServer server(config.getServerPort(), 
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
        
        std::cout << "Listening on port " << config.getServerPort() << std::endl;
        std::cout << "Server socket fd: " << server.get_server_fd() << std::endl;
        
        // 启动服务器事件循环
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "服务器启动失败: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}