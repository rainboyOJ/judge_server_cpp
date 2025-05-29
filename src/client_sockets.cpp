#include <sys/select.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <memory>

#include "Logger.h"
#include "judgeInfo.h"
#include "client_sockets.h"
#include "utils.h"


// 构造函数
ClientSockets::ClientSockets(testBox* test_box)
: test_box_(test_box)
{
    // client_sockets_.resize(test_box_->size() + 5);
    
    //初始化
    // for (auto & client_socket : client_sockets_)
    for (int i = 0;i< test_box_->size() + 5;i++)
    {
        client_sockets_.emplace_back(new FdInfo);
    }

    // 配置testBox_的回调函数
    // [!!流程6!!] 接收到测试点完成的回调的通知
    test_box_->setSingPointCompleteCallback(
        [this](int testBoxId)
        {
            // 设置为可以写,等待下一轮事件循环把这个对应的fd加入可以写的事件监听里
            // client_sockets_[testBoxId] -> writeAble = true;
            LOG_DEBUG("[client_socket recv testBox callback],set testBoxId %d writeAble\n",testBoxId);
            
            // TODO 这里修改一下,只让最后一次的测试点完成后,才设置写标志 
            // client_sockets_[testBoxId]->set_write_able();
        });

    test_box_->setallPointCompleteCallback(
        [this](int testBoxId) {
            // 设置为可以写,等待下一轮事件循环把这个对应的fd加入可以写的事件监听里
            LOG_DEBUG("[client_socket recv testBox callback],set all testBoxId %d completed\n",testBoxId);
            client_sockets_[testBoxId]->set_completed();
        }
    );
}


int ClientSockets::add_to_sets(fd_set &read_sets, fd_set &write_sets)
{
    int max_fd = 0;
    for (auto & client_socket : client_sockets_)
    {
        int fd = client_socket -> get_fd();
        if (fd == 0)
            continue; // 这个fd 没有值

        // !!注意这里只可以使用这种判断方式,不能使用函数获得对应的状态
        SocketStatus fd_status = client_socket -> get_status();
        // LOG_DEBUG("[in client_socket add_to_sets()] fd %d status %d", fd, (int)fd_status);
        if (fd_status == SocketStatus::readAble)
        {
            LOG_DEBUG("[in client_socket add_to_sets()]add socket %d to read_sets", fd);
            FD_SET(fd, &read_sets);
        }
        else if(fd_status == SocketStatus::writeAble || fd_status == SocketStatus::completed)
        {
            LOG_DEBUG("[in client_socket add_to_sets()]add socket %d to write_sets", fd);
            FD_SET(fd, &write_sets);
        }
        if ( fd > max_fd)
        {
            max_fd = fd;
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
        LOG_DEBUG("testBox is FULL,disconnect socket %d",client_socket);
        // TODO 发送评测已经满的信息
        //  并关闭连接
    }
    else {
        LOG_DEBUG("add socket %d to testBox as textBoxId %d", client_socket, testBoxId);
        client_sockets_[testBoxId]->init(client_socket);
    }
}

void ClientSockets::del_socket(int testBoxId)
{
    // 内部 close(fd)
    client_sockets_[testBoxId]->clear();

    //放回去
    test_box_->putBackTestBoxId(testBoxId);
    // TODO 是不是还有其它的需要处理?
}

void ClientSockets::deal_events(const fd_set &read_sets, const fd_set &write_sets)
{

    for (int i = 0; i < client_sockets_.size();i++)
    {
        int client_socket = client_sockets_[i]->get_fd();
        if( client_socket == 0) continue;
        int testBoxId = i;
        //读取事件
        if (FD_ISSET(client_socket, &read_sets))
        {
            LOG_DEBUG("read event on socket %d\n", client_socket);
            int bytes_read = 0;
            std::unique_ptr<testProblem> tp = client_sockets_[i]->read(bytes_read);
            if( bytes_read == 0) {
                LOG_INFO("Connection closed : %d\n",client_socket);
                del_socket(i);
            }
            else if( bytes_read < 0)
            {
                LOG_ERROR("Failed to read frome socket: %d\n",client_socket);
                del_socket(i);
            }
            else { //没有发生错误

#ifdef MUDEBUG
                // 此处输出解析后的testProblem,用于调试
                if (tp)
                {
                    std::cout << tp->pid << std::endl;
                    std::cout << tp->uuid << std::endl;
                    std::cout << tp->lang << std::endl;
                    std::cout << tp->code << std::endl;
                }
#endif
                LOG_DEBUG("deserializeTestProblem end!");

                // 传递给testBox

                testBox_err err = test_box_->add(
                    i, // testBoxId
                    std::move(tp));
                // TODO 处理错误,需要发送错误的信息给客户端
                if (err == testBox_err::PROBLEM_NOT_EXIST)
                {
                    LOG_ERROR("problem not exist");
                }
                else if (err == testBox_err::DATA_NOT_EXIST)
                {
                    LOG_ERROR("DATA_NOT_EXIST");
                }
                else if (err == testBox_err::SUCC)
                {
                    LOG_INFO("add testProblem to testBox SUCC\n");
                }
                // else if( err == testBox_err::FULL)
            }//没有读取发生错误else end
        }
        // 写事件
        else if( FD_ISSET(client_socket, &write_sets) )
        {
            LOG_DEBUG("write event on socket %d\n", client_socket);
            // 1. 先得到 FdInfo 的状态
            SocketStatus status = client_sockets_[i]->get_status();
            
            // 这个应该不会发生
            if( status == SocketStatus::testing) // 评测中
                continue;

            // 2. 得到testBox里的数据
            std::string result_data = this->test_box_->getResult(testBoxId);
#ifdef MUDEBUG
            // LOG_DEBUG("send %d bytes to socket %d", result_data.size(), client_socket);
            // debug_print_uint8_t_vector(result_data);
#endif
            int bytes_write = client_sockets_[i]->send(result_data);
            if( status == SocketStatus::completed)
            {
                // add : 2025-04-08
                //清空testBox对应resultContainer的数据
                this-> test_box_ -> clearResultByTestBoxId(testBoxId);


                client_sockets_[i]->set_read_able(); //转入read_able 的状态
            }
        }
    }
}

std::unique_ptr<testProblem> FdInfo::read(int &tot_read)
{
    char buf[1024];
    std::lock_guard lock(mtx_);

    // 先读取4个字节的数据,表示后面的数据的长度
    int ready_to_read = 0;
    int read_size = recv(fd, &ready_to_read, 4, 0);
    if( read_size <= 0) // 出错或者关闭连接
    {
        tot_read= read_size;
        LOG_DEBUG("read error or closed on socket %d,read return code %d", fd, read_size);
        return nullptr;
    }
    ready_to_read = ntohl(ready_to_read);
    LOG_DEBUG("ready_to_read = %d bytes\n", ready_to_read);

    //TODO 一次性读取数据,这里可能需要优化,
    // 1. 因为可能client 不会一次发送完整的数据过来
    // 所有fdinfo 需要一个buffer，这个buffer可以直接用muduo里buffer
    
    // 一直读取数据, 直到数据长度等于 ready_to_read
    tot_read = ready_to_read + 4;
    std::vector<uint8_t> read_data;
    while (ready_to_read > 0)
    {
        int size = recv(fd, buf, ready_to_read > 1024 ? 1024 : ready_to_read, 0);
        LOG_DEBUG("read %d bytes ...\n", size);
        if( size <= 0) // 出错或者关闭连接
        {
            //TODO 这里不能这样,需要一个读取缓冲区
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                continue;
            }

            tot_read = size;
            LOG_DEBUG("read error or closed on socket %d,read return code %d", fd, tot_read);
            return nullptr;
        }
        // read_data.append(buf, size);
        read_data.assign(buf, buf + size);
        ready_to_read -= size;
    }

    //TODO 这里有一个BUG,如果读取deserilize 数据失败
    //这里要进行处理,关闭 或返回一个信息,表示失败
    // 不能进行一步的测试

#ifdef MUDEBUG
    debug_print_uint8_t_vector(read_data);
#endif

    // 到这里, 已经读取了完整的数据
    status_ = SocketStatus::testing;

    // 智能检测数据格式并解析数据
    std::unique_ptr<testProblem> tp = nullptr;
    
    // 检测是否为JSON格式：查看第一个字符是否为 '{'
    if (read_data.size() > 0 && read_data[0] == '{') {
        // JSON格式处理
        LOG_DEBUG("Detected JSON format data\n");
        try {
            std::string json_str(read_data.begin(), read_data.end());
            LOG_DEBUG("JSON string: %s\n", json_str.c_str());
            
            tp = deserializeTestProblemFromJsonString(json_str);
            if (!tp) {
                LOG_ERROR("Failed to deserialize JSON data\n");
                return nullptr;
            }
            LOG_DEBUG("JSON deserializeTestProblem success\n");
        } catch (const std::exception& e) {
            LOG_ERROR("JSON parsing exception: %s\n", e.what());
            return nullptr;
        }
    } else {
        // 二进制格式处理（保持向后兼容）
        LOG_DEBUG("Using binary format deserialization\n");
        tp = std::make_unique<testProblem>();
        try {
            deserializeTestProblem(read_data.data(), *tp.get());
            LOG_DEBUG("Binary deserializeTestProblem success\n");
        } catch (const std::exception& e) {
            LOG_ERROR("Binary deserialization exception: %s\n", e.what());
            return nullptr;
        }
    }

    return std::move(tp);
    // deserializeTestProblem( read_data.data(), *tp.get());

        // // 3. 传递给testBox
    // test_box_->add(
    //     testBoxId,
    //     std::move(tp)
    // );
    // return tot_read;
}


//发送数据
// int ClientSockets::send_socket(int testBoxId, FdInfo& fd_info)
int FdInfo::send(std::string_view result_data)
{
    // 1. 从testBox上读取信息

    // TODO
    // std::vector<uint8_t> result_data = this->test_box_->getResult(testBoxId);
    // TODO 这里应该加一个highwater 阈值, 防止发送过快
    int send_size = result_data.size();
    LOG_DEBUG("ready to write %d result_data bytes to socket %d", send_size, fd);

    if( send_size <= 0) {
        LOG_ERROR("send size is 0, return");
        return 0; // 没有数据需要发送
    }

    int tot_send = send_size;
    // 先发送这个数据长度
    int snd_size = htonl(send_size);
    std::cout << "snd hex: " << std::hex << snd_size<< std::endl;
    int write_bytes = ::send(fd, &snd_size, sizeof(snd_size), 0);
    if (write_bytes <= 0)
        return write_bytes;
    
    // 这里不加了,以序列化后的result 数据大小为准
    // tot_send += write_bytes; 
    while (send_size > 0)
    {
        int size = ::send(fd, result_data.data() + (result_data.size() - send_size), send_size, 0);
        //TODO size < 0 出错
        if( size <= 0)
            return size;
        send_size -= size;
    }
    return tot_send;
}