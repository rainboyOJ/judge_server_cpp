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

#include "testBox.h" //与testBox 建立连接

class ClientSockets {

public:

    struct FdInfo {
        int fd;
        bool write; //是否可以写
        bool read; //是否可以读
    };

public:
    ClientSockets(testBox* test_box);
    void add_socket(int);

    //设置所有socket的加入对应的监听，并返回最大的那个socket
    int add_to_sets(fd_set & read_sets, fd_set& write_sets);

    //得到id对应的fd
    int id_to_fd(const int id) const {return client_sockets_[id].fd;};

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
    std::vector<FdInfo> client_sockets_;

};