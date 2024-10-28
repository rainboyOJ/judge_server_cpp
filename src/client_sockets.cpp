#include <sys/select.h>
#include <memory>

#include "Logger.h"
#include "judgeInfo.h"
#include "client_sockets.h"

// 构造函数
ClientSockets::ClientSockets(testBox* test_box)
: test_box_(test_box)
{
    client_sockets_.reserve(test_box_->size() + 5);
    for (auto & client_socket : client_sockets_)
    {
        client_socket.fd = 0;
        client_socket.write = false;
        client_socket.read = false;
    }
}


int ClientSockets::add_to_sets(fd_set &read_sets, fd_set &write_sets)
{
    int max_fd = 0;
    for (auto & client_socket : client_sockets_)
    {
        if( client_socket.fd == 0 ) continue;
        if(client_socket.read)
            FD_SET(client_socket.fd, &read_sets);
        if(client_socket.write)
            FD_SET(client_socket.fd, &write_sets);
        if ( client_socket.fd > max_fd)
        {
            max_fd = client_socket.fd;
        }
    }
    return max_fd;
}

void ClientSockets::add_socket(int client_socket)
{
    // 先得到
    // client_sockets_.push_back(client_socket);
    int testBoxId = test_box_->getTestBoxId();
    if( testBoxId == -1) {
        //TODO 发送评测已经满的信息
        // 并关闭连接
    }
    else {
        client_sockets_[testBoxId].fd = client_socket;
        client_sockets_[testBoxId].write = false;
        client_sockets_[testBoxId].read = true;
    }
}

void ClientSockets::del_socket(int testBoxId)
{
    close(client_sockets_[testBoxId].fd);
    client_sockets_[testBoxId].write = false;
    client_sockets_[testBoxId].read = false;
    
    //放回去
    test_box_->putBackTestBoxId(testBoxId);
    // TODO 是不是还有其它的需要处理?
}

void ClientSockets::deal_events(const fd_set &read_sets, const fd_set &write_sets)
{

    for (int i = 0; i < client_sockets_.size();i++)
    {
        int client_socket = client_sockets_[i].fd;
        //读取事件
        if( FD_ISSET(client_socket, &read_sets) )
        {
            int bytes_read = read_socket(i, client_sockets_[i]);
            // 处理错误
            if( bytes_read == 0) {
                LOG_INFO("Connection closed : %d\n",client_socket);
                del_socket(i);
            }
            else if( bytes_read < 0)
            {
                LOG_ERROR("Failed to read frome socket: %d\n",client_socket);
                del_socket(i);
            }
        }
        // 写事件
        else if( FD_ISSET(client_socket, &write_sets) )
        {
            int bytes_write= send_socket(i, client_sockets_[i]);
        }
    }
}

int ClientSockets::read_socket(int testBoxId, FdInfo& fd_info)
{
    std::vector<uint8_t> read_data;
    char buf[1024];

    // 先读取4个字节的数据,表示后面的数据的长度
    int ready_to_read = 0;
    int read_size = recv(fd_info.fd, &ready_to_read, 4, 0);
    if( read_size <= 0) // 出错或者关闭连接
        return read_size;
    ready_to_read = ntohl(ready_to_read);

    //TODO 一次性读取数据,这里可能需要优化,
    // 1. client 不会一次发送完整的数据过来
    
    // 一直读取数据, 直到数据长度等于 ready_to_read
    int tot_read = ready_to_read;
    while (ready_to_read > 0)
    {
        int size = recv(fd_info.fd, buf, ready_to_read > 1024 ? 1024 : ready_to_read, 0);
        if( size <= 0) // 出错或者关闭连接
            return size;
        // read_data.append(buf, size);
        read_data.assign(buf, buf + size);
        ready_to_read -= size;
    }

    // 2. 把数据转成 protobuf 数据
    // TODO 数据置换
    // judge_message_for_transport::recTestInfo rec_test_info;
    // rec_test_info.ParseFromString(read_data);

    // 解析 数据进行测试
    std::unique_ptr<testProblem> tp = std::make_unique<testProblem>();
    deserializeTestProblem( read_data.data(), *tp.get());

    
    // 3. 传递给testBox
    test_box_->add(
        testBoxId,
        std::move(tp)
    );
    return tot_read;
}


//发送数据
int ClientSockets::send_socket(int testBoxId, FdInfo& fd_info)
{
    // 从testBox上读取信息


}