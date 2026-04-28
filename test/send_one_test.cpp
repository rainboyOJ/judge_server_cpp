//功能: 发送一条消息的测试信息
//

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "legacy/judgeInfo.h"
#include "utils.h"

int main(int argc, char const *argv[])
{
    // 使用socket 连接服务器
    // 1.创建一个socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // 2.连接服务器
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if( connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0 )
    {
        std::cerr << "Connection failed" << std::endl;
        close(sockfd); // 关闭 socket
        return -1;
    }
    std::cout << "Connection successful!" << std::endl;

    // 3.发送消息

    //3.1 创建一个testProblem
    testProblem tp;
    tp.code = "print('Hello, world!')";
    tp.lang = language::cpp;
    memset(&tp.pid, 0, sizeof(tp.pid));
    strcpy(tp.pid, "1000");
    tp.uuid = 1;
    // 3.2 序列化testProblem
    std::vector<uint8_t> serialized_tp = serializeTestProblem(tp);

    // char msg[] = "Hello, world!";
    // send(sockfd, msg, strlen(msg), 0);
    int send_size = serialized_tp.size();
    int write = htonl(send_size);
    send(sockfd, &write, sizeof(write), 0);
    std::cout << "send test_problem size: " << send_size << " bytes"<< std::endl;
    std::cout << "send test_problem content: \n" << std::endl;

    debug_print_uint8_t_vector(serialized_tp);

    int tot_send = 0;
    while (tot_send < send_size)
    {
        int write_bytes = send(sockfd, serialized_tp.data()+tot_send, send_size-tot_send, 0);
        if( write_bytes <= 0)
        {
            std::cout << "send error" << std::endl;
            break;
        }
        tot_send += write_bytes;
    }
    std::cout << "send test_problem finished,then read from server" << std::endl;

    int read_size = 0;
    ::read(sockfd, &read_size, sizeof(read_size));
    //DEBUG read_size hex
    // std::cout << "read_size hex: " << std::hex << read_size << std::endl;
    read_size = ntohl(read_size);
    std::cout << "ready to read test_result size: " << std::dec<< read_size << " bytes"<< std::endl;

    int tot_read = read_size;

    std::vector<uint8_t> recv_testResult_buf(read_size);
    while (tot_read > 0)
    {

        char recv_buf[1024];
        int recv_bytes = recv(sockfd, recv_buf, 1024, 0);
        if( recv_bytes <= 0)
        {
            std::cout << "recv error" << std::endl;
            break;
        }
        recv_testResult_buf.assign(recv_buf, recv_buf + recv_bytes);
        tot_read -= recv_bytes;
    }
    std::cout << "recv test_result finished, tot_read : " << read_size << std::endl;
    debug_print_uint8_t_vector(recv_testResult_buf);

    // 反序列化结果
    testResultWithVecotr testResult;
    deserializeTestPointResult(recv_testResult_buf.data(), testResult);

    std::cout << "Result info :\n";
    std::cout << "uuid: " << testResult.uuid << std::endl;
    std::cout << "testBoxId " << testResult.testBoxId << std::endl;
    std::cout << "err_type " << testResult.err_type << std::endl;
    std::cout << "lang " << testResult.lang<< std::endl;
    for(auto& point_result : testResult.trp)
    {
        std::cout << std::dec << "seq_id: " << point_result.seq_id
                  << " cpu_time: " << point_result.cpu_time
                  << " real_time: " << point_result.real_time
                  << " memory: " << point_result.memory
                  << " signal: " << point_result.signal
                  << " exit_code: " << point_result.exit_code
                  << " error: " << point_result.error
                  << " result: " << point_result.result << std::endl;
    }

    // 4.关闭socket
    close(sockfd);

    return 0;
}
