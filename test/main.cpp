#include "legacy/testPointBox.h"
#include <cstring>
#include <iostream>
#include <chrono>
char id_[] = "p1001";

int main (int argc, char *argv[]) {
    //创建一个testPointBox
    testPointBox tpb(3,nullptr);
    // 添加任务

    // 得到5个testPoint
    testPoint * testPointLink = tpb.get_testPoint_link(5);
    testPoint *head = testPointLink;

    // 设置任务
    for(int i =1;i<=5;i++) {
        head -> seq_id = i;
        head -> lang = language::cpp;
        sprintf(head->input_path,"input/input%d.txt",i);
        sprintf(head->output_path,"output/output%d.txt",i);
        strcpy(head->exe, "/usr/bin/ls");

        head->testPointResult_p = nullptr;

        head = head->nxt;
        std::cout << "create test point " << i  << "\n";
    }
    tpb.push_link(testPointLink);
    return 0;
}
