好的,这里是使用 C++ 实现相同功能的示例代码,并附有详细注释:

```cpp
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
#include <poll.h>

// 消息队列
std::queue<std::string> message_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;

// 工作线程函数
void worker_thread() {
    while (true) {
        std::string message;
        {
            // 从消息队列中获取一个消息
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, []{return !message_queue.empty();});
            message = message_queue.front();
            message_queue.pop();
        }

        // 模拟处理消息的计算过程
        int result = process_message(message);

        // 将计算结果放回消息队列
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            message_queue.push(std::to_string(result));
            queue_cv.notify_one();
        }
    }
}

// 处理消息的函数
int process_message(const std::string& message) {
    return message.length();
}

// 主函数
int main() {
    // 创建工作线程
    std::vector<std::thread> worker_threads;
    for (int i = 0; i < 4; i++) {
        worker_threads.emplace_back(worker_thread);
    }

    // 创建 socket 服务器
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

    // 设置 socket 为非阻塞模式
    fcntl(server_socket, F_SETFL, O_NONBLOCK);

    // 绑定 socket 到本地地址
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
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

    // 主循环,监听 socket 事件并处理
    std::vector<pollfd> pollfds = {{server_socket, POLLIN, 0}};
    while (true) {
        int ready = poll(pollfds.data(), pollfds.size(), -1);
        if (ready == -1) {
            std::cerr << "Poll failed" << std::endl;
            return 1;
        }

        if (pollfds[0].revents & POLLIN) {
            // 接受新的连接请求
            int client_socket = accept(server_socket, nullptr, nullptr);
            if (client_socket == -1) {
                std::cerr << "Failed to accept connection" << std::endl;
                continue;
            }

            // 设置客户端 socket 为非阻塞模式
            fcntl(client_socket, F_SETFL, O_NONBLOCK);

            // 将客户端 socket 添加到 poll 列表中
            pollfds.push_back({client_socket, POLLIN, 0});
            std::cout << "Accepted connection" << std::endl;
        }

        for (size_t i = 1; i < pollfds.size(); i++) {
            if (pollfds[i].revents & POLLIN) {
                // 读取客户端发送的消息
                char buffer[1024];
                ssize_t bytes_read = read(pollfds[i].fd, buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    std::string message(buffer, bytes_read);
                    std::cout << "Received message: " << message << std::endl;

                    // 将消息放入消息队列
                    {
                        std::lock_guard<std::mutex> lock(queue_mutex);
                        message_queue.push(message);
                        queue_cv.notify_one();
                    }

                    // 将客户端 socket 添加到 poll 列表中,监听写事件
                    pollfds[i].events = POLLOUT;
                }
                else if (bytes_read == 0) {
                    // 客户端断开连接
                    std::cout << "Connection closed" << std::endl;
                    close(pollfds[i].fd);
                    pollfds.erase(pollfds.begin() + i);
                    i--;
                }
                else {
                    std::cerr << "Failed to read from socket" << std::endl;
                    close(pollfds[i].fd);
                    pollfds.erase(pollfds.begin() + i);
                    i--;
                }
            }
            else if (pollfds[i].revents & POLLOUT) {
                // 发送计算结果给客户端
                std::string result;
                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    result = message_queue.front();
                    message_queue.pop();
                }
                ssize_t bytes_written = write(pollfds[i].fd, result.c_str(), result.length());
                if (bytes_written == -1) {
                    std::cerr << "Failed to write to socket" << std::endl;
                    close(pollfds[i].fd);
                    pollfds.erase(pollfds.begin() + i);
                    i--;
                }
                else {
                    // 从 poll 列表中移除该客户端 socket
                    pollfds.erase(pollfds.begin() + i);
                    i--;
                }
            }
        }
    }

    // 等待工作线程退出
    for (auto& thread : worker_threads) {
        thread.join();
    }

    return 0;
}
```

注释:

1. 定义一个消息队列,用于在主线程和工作线程之间传递数据。使用 `std::mutex` 和 `std::condition_variable` 进行线程安全的访问。
2. 定义一个工作线程函数,用于处理从消息队列中获取的消息。从队列中获取消息,处理消息,并将计算结果放回队列。
3. 定义一个处理消息的函数,这里只是简单地返回消息的长度。
4. 在 `main` 函数中创建工作线程。
5. 创建一个 socket 服务器,并将其设置为非阻塞模式。
6. 进入主循环,使用 `poll` 函数监听 socket 事件并处理。
   - 当有新的连接请求时,接受连接并将客户端 socket 添加到 `pollfds` 列表中。
   - 当有客户端发送消息时,读取消息并将其放入消息队列。将客户端 socket 添加到 `pollfds` 列表中,监听写事件。
   - 当有客户端 socket 可写时,从消息队列中获取计算结果,并发送给客户端。从 `pollfds` 列表中移除该客户端 socket。
7. 等待工作线程退出。

这个示例实现了与 Python 版本相同的功能,使用 C++ 的 `poll` 函数监听 socket 事件,并使用工作线程池进行多线程计算。主线程负责接收和发送信息,工作线程负责处理消息。
