// 对 testProblem 的序列化和反序列化进行测试

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cassert>

#include "judgeInfo.h"

int main(int argc, char const *argv[])
{
    //1. 创建一个testProblem
    testProblem tp;
    tp.code = "print('Hello, world!')";
    tp.lang = language::cpp;
    memset(&tp.pid, 0, sizeof(tp.pid));
    strcpy(tp.pid, "1000");
    tp.uuid = 1;
    // 3.2 序列化testProblem
    std::vector<uint8_t> serialized_tp = serializeTestProblem(tp);

    debug_print_uint8_t_vector(serialized_tp);

    // 解码

    testProblem tp2;
    deserializeTestProblem(serialized_tp.data(), tp2);
    assert(tp.code == tp2.code);
    assert(tp.lang == tp2.lang);
    assert(strcmp(tp.pid, tp2.pid) == 0);
    assert(tp.uuid == tp2.uuid);

    // 输出

    return 0;
}
