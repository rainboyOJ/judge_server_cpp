处理客户端连接

每一次client socket连接建立的时候，会调用`testBox->getTestBoxId()`来获取一个唯一的id,这个id会随着连接的关闭而失效(即连接断开后,id会被回收).

这个id可以用来区分不同的client,比如在同一个testBox中,可以通过id来区分不同的client,从而实现不同的功能.

client就是通过这个id来进行通信的，从而实现发现评测数据，获取评测结果。

一个client 的生命周期内，它的id是**固定的**。


```mermaid
classDiagram
    class ClientSockets {
        + ClientSockets(testBox*)
        + void add_socket(int)
        + int add_to_sets(fd_set &, fd_set&)
        + int id_to_fd(const int) const
        + void deal_events(const fd_set&, const fd_set &)
        + void del_socket(int)
        - int read_socket(int, FdInfo&)
        - int send_socket(int, FdInfo&)
        - testBox* test_box_
        - std::vector~std::unique_ptr~FdInfo~~ client_sockets_
    }

    class testBox
    class FdInfo {
        + FdInfo()
        + int get_fd() const
        + void set_fd(int)
        + void set_status(SocketStatus)
        + void init(int)
        + void clear()
        + SocketStatus get_status()
        + bool is_read_able()
        + bool is_write_able()
        + bool is_testing()
        + bool is_completed()
        + void set_wirte_able()
        + void set_read_able()
        + void set_testing()
        + void set_completed()
        + std::unique_ptr~testProblem~ read(int &)
        + int send(const std::vector<uint8_t>&)
        - int fd
        - std::mutex mtx_
        - SocketStatus status_
    }
    class testProblem
    class SocketStatus {
        readAble
        testing
        writeAble
        completed
    }

    ClientSockets "1" *-- "1" testBox : has a
    ClientSockets "1" *-- "0..*" FdInfo : has many
    FdInfo "1" -- "1" SocketStatus : has a
    FdInfo "1" -- "1" testProblem : uses
```

## 事件循环

这里详细的解释一下事件循环的流程与FdInfo的各个状态的转变。

![](./drawio/event_loop.svg)

```
main 循环
```