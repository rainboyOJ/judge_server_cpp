以下是一个使用 C++ 的 `select` 和 `socket` 实现的网络编程示例代码，满足您所描述的功能，并附有详细注释：

```cpp
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

std::queue<std::string> messageQueue; // 消息队列
std::mutex queueMutex; // 互斥锁
std::condition_variable cv; // 条件变量
bool done = false; // 标志，表示工作线程是否完成

// 工作线程函数
void worker_thread() {
    while (true) {
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, [] { return !messageQueue.empty() || done; }); // 等待消息或完成信号

        if (done && messageQueue.empty()) {
            break; // 如果完成且队列为空，退出线程
        }

        // 处理消息
        std::string message = messageQueue.front();
        messageQueue.pop();
        lock.unlock();

        // 模拟处理消息的计算过程
        std::string result = "Processed: " + message;

        // 这里可以将结果发送回主线程或其他处理
        std::cout << result << std::endl;
    }
}

// 接受连接的函数
void accept_connection(int server_socket) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
    if (client_socket < 0) {
        perror("accept");
        return;
    }

    std::cout << "Accepted connection" << std::endl;

    // 设置非阻塞模式
    fcntl(client_socket, F_SETFL, O_NONBLOCK);

    // 读取消息
    char buffer[1024];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0'; // 添加字符串结束符
        std::cout << "Received message: " << buffer << std::endl;

        // 将消息放入消息队列
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            messageQueue.push(buffer);
        }
        cv.notify_one(); // 通知工作线程有新消息
    }

    close(client_socket); // 关闭客户端连接
}

int main() {
    // 创建服务器 socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return 1;
    }

    // 绑定 socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8000);
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }

    // 监听连接
    listen(server_socket, 5);

    // 创建工作线程
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back(worker_thread);
    }

    // 主循环，接受连接
    fd_set read_fds;
    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);

        // 设置超时时间
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // 选择可读的 socket
        int activity = select(server_socket + 1, &read_fds, nullptr, nullptr, &timeout);
        if (activity < 0) {
            perror("select");
            break;
        }

        // 如果有新的连接
        if (FD_ISSET(server_socket, &read_fds)) {
            accept_connection(server_socket);
        }
    }

    // 结束工作线程
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        done = true;
    }
    cv.notify_all(); // 通知所有工作线程退出

    for (auto& worker : workers) {
        worker.join(); // 等待所有工作线程结束
    }

    close(server_socket); // 关闭服务器 socket
    return 0;
}
```

### 代码说明：

1. **消息队列**：使用 `std::queue` 存储接收到的消息，并使用 `std::mutex` 和 `std::condition_variable` 来保护和通知工作线程。

2. **工作线程**：每个工作线程从消息队列中获取消息并处理。处理过程在这里简单模拟为返回处理后的字符串。

3. **接受连接**：`accept_connection` 函数接受新的客户端连接，读取消息并将其放入消息队列。

4. **主函数**：
   - 创建服务器 socket，绑定到指定端口并开始监听。
   - 创建多个工作线程。
   - 使用 `select` 监听服务器 socket 的可读事件，接受新的连接。

5. **线程结束**：在主循环结束后，设置 `done` 标志并通知所有工作线程退出。

这个示例实现了一个简单的多线程网络服务器，能够接收客户端消息并通过工作线程进行处理。
