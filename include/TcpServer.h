#pragma once
#include <functional>
#include <sys/select.h>

class TcpServer {
public:
    using AcceptCallback = std::function<void(int client_fd)>;
    using ClientEventCallback = std::function<void(fd_set&, fd_set&)>;
    using ReNewSocketSets = std::function<void(fd_set&, fd_set&)>;

    // 构造函数：创建并初始化服务器socket
    // 参数：port 监听端口，on_accept 接受连接回调，on_client_event 客户端事件回调
    //      nonblock 是否非阻塞，reuseaddr 是否复用地址
    TcpServer(int port, 
                AcceptCallback on_accept, 
                ClientEventCallback on_client_event = nullptr, 
                // 加入socket到fd_set中
                ReNewSocketSets renew_socket_sets = nullptr,
              bool nonblock = true, bool reuseaddr = true);
    
    ~TcpServer();

    // 启动事件循环
    void start();
    
    // 停止服务器
    void stop();
    
    // 获取服务器socket fd
    int get_server_fd() const { return server_fd_; }
    
    // 检查服务器是否正在运行
    bool is_running() const { return running_; }

private:
    int server_fd_;
    bool running_;
    AcceptCallback on_accept_;
    ClientEventCallback on_client_event_;
    ReNewSocketSets renew_socket_sets_;
    
    // 创建并配置服务器socket
    bool create_and_bind_socket(int port, bool nonblock, bool reuseaddr);
};
