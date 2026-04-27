/**
 * @file main.cpp
 * @brief 异步评测后端的程序入口。
 *
 * 当前主程序负责组装：
 * - TCP/select 网络外壳
 * - SubmissionQueue 任务队列
 * - JudgeWorkerPool 后台 worker
 * - ClientSockets 协议接入与回包协调
 */

#include <condition_variable>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <sys/select.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "TcpServer.h"
#include "client_sockets.h"
#include "common/Config.h"
#include "common/Logger.h"
#include "dispatch/JudgeWorkerPool.h"
#include "dispatch/SubmissionQueue.h"
#include "testBox.h"

int main(int argc, char *argv[]) {
  // 解析命令行参数
  std::string config_file = "config/config.json"; // 默认配置文件路径

  if (argc >= 2) {
    config_file = argv[1];
  }

  // 加载配置
  Config &config = Config::getInstance();
  config.loadFromFile(config_file);

  std::cout << "=== BoxTest Server Starting ===" << std::endl;
  std::cout << "Config file: " << config_file << std::endl;
  std::cout << "Server port: " << config.getServerPort() << std::endl;
  std::cout << "Worker threads: " << config.getWorkerThreadCount() << std::endl;
  std::cout << "Max concurrent tests: " << config.getMaxConcurrentTests()
            << std::endl;
  std::cout << "Test data path: " << config.getTestDataPath() << std::endl;
  std::cout << "================================" << std::endl;

  // 1. 创建testBox，使用配置中的参数
  std::unique_ptr<testBox> test_box = std::make_unique<testBox>(
      config.getWorkerThreadCount(), config.getMaxConcurrentTests(),
      config.getTestDataPath());
  SubmissionQueue submission_queue;

  try {
    ClientSockets *client_sockets_ptr = nullptr;
    std::unique_ptr<TcpServer> server;
    std::function<void()> wake_server;
    ClientSockets client_sockets(test_box.get(), submission_queue,
                                 [&wake_server]() {
                                    if (wake_server) {
                                      wake_server();
                                   }
                                 });
    JudgeWorkerPool judge_worker_pool(
        config.getWorkerThreadCount(), submission_queue,
        client_sockets.submission_service(), &client_sockets);
    client_sockets_ptr = &client_sockets;

    // 创建TCP服务器，使用配置中的端口
    server = std::make_unique<TcpServer>(
        config.getServerPort(),
        // 接受新连接的回调
        [&client_sockets_ptr](int client_fd) {
          client_sockets_ptr->add_socket(client_fd);
          LOG_INFO("Accepted connection, fd: %d", client_fd);
        },
        // 处理客户端事件的回调
        [&client_sockets_ptr](fd_set &read_fds, fd_set &write_fds) {
          client_sockets_ptr->deal_events(read_fds, write_fds);
        },
        // 加入socket到fd_set中
        [&client_sockets_ptr](fd_set &read_fds, fd_set &write_fds) {
          client_sockets_ptr->add_to_sets(read_fds, write_fds);
        });
    wake_server = [server_ptr = server.get()]() { server_ptr->wake(); };

    std::cout << "Listening on port " << config.getServerPort() << std::endl;
    std::cout << "Server socket fd: " << server->get_server_fd() << std::endl;

    // 启动服务器事件循环
    server->start();

  } catch (const std::exception &e) {
    std::cerr << "服务器启动失败: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
