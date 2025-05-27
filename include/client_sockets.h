// 连接上层服务器主循环线程 与 testBox，
// 1. 加入，删除，client_socket
// 2. 设置socket的状态，可读，可写
// 3. 发送，读取，信息
// 4. 设置 外部的FD_SET
#pragma once

#include <iostream>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#include <mutex>

#include "testBox.h" //与testBox 建立连接

// socket 进行测试的状态
enum class SocketStatus {
    readAble, // 准备好进行测试,此时可以读取数据
    testing, // 正在进行测试,此时不能进行读写操作
    writeAble, // 已经得到测试点的数据,可以进行写数据
    completed // 已经测试完毕,可以发数据
};

class FdInfo
{
public:
    FdInfo() :fd(0), status_(SocketStatus::readAble) { }

    int get_fd() const { return fd; }

    void set_fd(int fd_) { fd = fd_;}
    void set_status(SocketStatus status) { status_ = status; }

    // 初始化
    void init(int fd_) {
        // TODO 其时这里完全不用锁，因为根据这个函数使用时机
        std::lock_guard<std::mutex> lock(mtx_);
        fd = fd_;
        status_ = SocketStatus::readAble;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        if( fd !=0 ) close(fd);
        fd = 0;
        status_ = SocketStatus::readAble;
    }

    SocketStatus get_status() {
        std::lock_guard<std::mutex> lock(mtx_);
        return status_;
    }

    bool is_read_able() {
        return get_status() == SocketStatus::readAble;
    }
    bool is_write_able() {
        return get_status() == SocketStatus::writeAble;
    }
    bool is_testing() {
        return get_status() == SocketStatus::testing;
    }
    bool is_completed() {
        return get_status() == SocketStatus::completed;
    }

    void set_write_able() {
        std::lock_guard<std::mutex> lock(mtx_);
        status_ = SocketStatus::writeAble;
    }

    void set_read_able() {
        std::lock_guard<std::mutex> lock(mtx_);
        status_ = SocketStatus::readAble;
    }

    void set_testing() {
        std::lock_guard<std::mutex> lock(mtx_);
        status_ = SocketStatus::testing;
    }

    void set_completed() {
        std::lock_guard<std::mutex> lock(mtx_);
        status_ = SocketStatus::completed;
    }


    //读取数据
    std::unique_ptr<testProblem> read(int &read_size);

    //发送数据
    int send(const std::vector<uint8_t>& data);

private:
    int fd;
    std::mutex mtx_;
    SocketStatus status_;
};

class ClientSockets {
public:
    ClientSockets(testBox* test_box);
    void add_socket(int);

    //设置所有socket的加入对应的监听，并返回最大的那个socket
    int add_to_sets(fd_set & read_sets, fd_set& write_sets);

    //得到testBoxId对应的fd
    int id_to_fd(const int id) const {return client_sockets_[id] -> get_fd();};

    // 处理对应的事件
    void deal_events(const fd_set& read_sets,const fd_set & write_sets);

    // 删除一个socket
    void del_socket(int testBoxId);

private:

    //读取发来的评测信息
    int read_socket(int testBoxId,FdInfo& fd_info);
    // 发送评测信息
    int send_socket(int testBoxId,FdInfo& fd_info);

    testBox* test_box_; //指向testBox的指针
    std::vector< std::unique_ptr<FdInfo> > client_sockets_; // 保存所有的socket信息

};