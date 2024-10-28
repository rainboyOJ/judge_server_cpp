//测试 静态数组循环队列

#include <iostream>
#include "static_loop_queue.h"
using namespace std;

int main(int argc, char const *argv[])
{
    StaticLoopQueue<int> que(5);
    for (int i = 1; i <= 6;i++) {
        cout << " push  : " << i << ": ";
        bool ret = que.push(i);
        if (ret) {
            cout << "success" << endl;
        } else {
            cout << "failed" << endl;
        }
    }

    que.pop();
    que.pop();
    que.pop();

    for (int i = 7; i <= 7+4;i++) {
        cout << " push  : " << i << ": ";
        bool ret = que.push(i);
        if (ret) {
            cout << "success" << endl;
        } else {
            cout << "failed" << endl;
        }
    }

    while( !que.empty())
    {
        cout << " pop  : " << que.pop() << endl;
    }
    return 0;
}
