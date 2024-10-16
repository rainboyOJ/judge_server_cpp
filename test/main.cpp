#include "testPointBox.h"
#include <cstring>
#include <iostream>
#include <chrono>
char id_[] = "p1001";

int main (int argc, char *argv[]) {
    //创建一个testPointBox
    testPointBox tpb(1);
    // 添加人物
    std::this_thread::sleep_for(std::chrono::seconds(3));
    for(int i =1;i<=5;i++) {
        testPoint * t = new testPoint;
        t->seq_id = i;
        // t->id_ = "123";
        strcpy(t->id_,id_);
        // std::cout << "create test point " << i  << "\n";
        tpb.push(std::unique_ptr<testPoint> (t));
    }
    return 0;
}
