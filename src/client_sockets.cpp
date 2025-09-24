#include <sys/select.h>

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <memory>

#include "common/Logger.h"
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
            // LOG_DEBUG("[client_socket recv testBox callback],set testBoxId %d writeAble\n",testBoxId);
            
            // TODO 这里修改一下,只让最后一次的测试点完成后,才设置写标志 
            // client_sockets_[testBoxId]->set_writable();
        });

    test_box_->setallPointCompleteCallback(
        [this](int testBoxId) {
            // 设置为可以写,等待下一轮事件循环把这个对应的fd加入可以写的事件监听里
            LOG_DEBUG("[client_socket recv testBox callback],set all testBoxId %d completed\n",testBoxId);
            client_sockets_[testBoxId]->set_writable();
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

        // 使用新的状态判断方式
        if (client_socket->is_readable())
        {
            // LOG_DEBUG("[in client_socket add_to_sets()]add socket %d to read_sets", fd);
            FD_SET(fd, &read_sets);
        }
        else if(client_socket->is_writable())
        {
            // LOG_DEBUG("[in client_socket add_to_sets()]add socket %d to write_sets", fd);
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

    // comment by rainboy
    // 这里 用到了 private testbox::getTestBoxId() ,后面需要修改,先注释掉
    // int testBoxId = test_box_->getTestBoxId();
    // if( testBoxId == -1) {
    //     LOG_DEBUG("testBox is FULL,disconnect socket %d",client_socket);
    //     // TODO 发送评测已经满的信息
    //     //  并关闭连接
    // }
    // else {
    //     LOG_DEBUG("add socket %d to testBox as textBoxId %d", client_socket, testBoxId);
    //     client_sockets_[testBoxId]->init(client_socket);
    // }
}

void ClientSockets::del_socket(int testBoxId)
{
    // 内部 close(fd)
    client_sockets_[testBoxId]->clear();

    //放回去
    // comment by rainboy
    // 这里 用到了 private testbox::putBackTestBoxId() ,后面需要修改,先注释掉
    // test_box_->putBackTestBoxId(testBoxId);
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
            else if ( tp != nullptr) { //没有发生错误

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

                // comment by rainboy
                // 这里 用到了 修改后的 testbox::add() ,后面需要修改,先注释掉
                // TestBoxVoidResult result = test_box_->add(
                //     i, // testBoxId
                //     std::move(tp));
                // // TODO 处理错误,需要发送错误的信息给客户端
                // if (result.isFailure())
                // {
                //     TestBoxError error = result.error();
                //     LOG_ERROR("TestBox add failed: %s", error.data());  
                // }
                // else
                // {
                //     LOG_INFO("add testProblem to testBox SUCCESS\n");
                // }
            }//没有读取发生错误else end
            else {
                LOG_DEBUG("read bytes_read = %d, but not get All testProblem data", bytes_read);
            }
        }
        // 写事件
        else if( FD_ISSET(client_socket, &write_sets) )
        {
            LOG_DEBUG("write event on socket %d\n", client_socket);
            // 1. 检查socket状态，确保可写
            if(!client_sockets_[i]->is_writable()) // 不是可写状态
                continue;

            // 2. 得到testBox里的数据
            std::string result_data = this->test_box_->getResult(testBoxId);
#ifdef MUDEBUG
            // LOG_DEBUG("send %d bytes to socket %d", result_data.size(), client_socket);
            // debug_print_uint8_t_vector(result_data);
#endif
            int bytes_write = client_sockets_[i]->send(result_data);
            
            // 发送完成后，清空testBox对应resultContainer的数据并设置为可读状态
            //清空testBox对应resultContainer的数据
            this-> test_box_ -> clearResultByTestBoxId(testBoxId);

            client_sockets_[i]->set_readable(); //转入readable 的状态
        }
    }
}

std::unique_ptr<testProblem> FdInfo::read(int &tot_read)
{
     //这里不需要锁,因为read 函数是单线程的
    // std::lock_guard lock(mtx_);
    tot_read = 0; // 初始化 tot_read

    // 从socket读取数据到input_buffer_
    int saved_errno = 0;
    ssize_t n = input_buffer_.readFd(fd, &saved_errno);
    tot_read = n;

    if (n < 0) { // 读取出错
        if (saved_errno != EAGAIN && saved_errno != EWOULDBLOCK) {
            LOG_ERROR("FdInfo::read error on socket %d, errno: %d", fd, saved_errno);
            tot_read = -1; // 表示读取错误
        }
        // 如果是 EAGAIN 或 EWOULDBLOCK，表示数据未准备好，不是错误，tot_read 保持 0
        return nullptr;
    }
    if (n == 0) { // 对端关闭连接
        LOG_INFO("Connection closed by peer on socket %d", fd);
        tot_read = 0; // 表示连接关闭，但不是读取错误，所以不设为-1
        return nullptr;
    }

    // 检查input_buffer_中是否有足够的数据解析出一个完整的消息
    // 消息格式：4字节长度前缀 (网络字节序) + 消息体
    if (input_buffer_.readableBytes() < sizeof(uint32_t)) {
        // 长度前缀都还没收到，等待更多数据
        return nullptr;
    }

    uint32_t msg_len_net;
    // 查看 (peek) 长度前缀，但不消耗它
    memcpy(&msg_len_net, input_buffer_.peek(), sizeof(uint32_t));
    uint32_t msg_len_host = ntohl(msg_len_net);

    LOG_DEBUG("Expected message body length = %u bytes\n", msg_len_host);

    if (input_buffer_.readableBytes() < sizeof(uint32_t) + msg_len_host) {
        // 消息体不完整，等待更多数据
        LOG_DEBUG("Incomplete message: readableBytes()=%zu, needed=%zu", input_buffer_.readableBytes(), sizeof(uint32_t) + msg_len_host);
        return nullptr;
    }

    // 消耗掉长度前缀
    input_buffer_.retrieve(sizeof(uint32_t));

    // 读取消息体
    std::string msg_body_str = input_buffer_.retrieveAsString(msg_len_host);
    // tot_read = sizeof(uint32_t) + msg_len_host; // 更新实际读取的总字节数

#ifdef MUDEBUG
    LOG_DEBUG("Received message body: %s", msg_body_str.c_str());
#endif

    // 到这里, 已经读取了完整的数据
    set_idle(); // 设置为不可读写状态
    

    // 智能检测数据格式并解析数据
    std::unique_ptr<testProblem> tp = nullptr;
    
    // 检测是否为JSON格式：查看第一个字符是否为 '{'
    if (!msg_body_str.empty() && msg_body_str[0] == '{') {
        LOG_DEBUG("Detected JSON format data\n");
        try {
            tp = deserializeTestProblemFromJsonString(msg_body_str);
            if (!tp) {
                LOG_ERROR("Failed to deserialize JSON data\n");
                set_readable(); // 反序列化失败，重置为可读状态
                return nullptr;
            }
            LOG_DEBUG("JSON deserializeTestProblem success\n");
        } catch (const std::exception& e) {
            LOG_ERROR("JSON parsing exception: %s\n", e.what());
            set_readable(); // 解析异常，重置为可读状态
            return nullptr;
        }
    } else {
        LOG_ERROR("Unknown data format or empty message body received.");
        set_readable(); // 未知格式，重置为可读状态
        return nullptr;
    }
    
    return tp; // 使用 std::move 不是必要的，因为有 NRVO
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