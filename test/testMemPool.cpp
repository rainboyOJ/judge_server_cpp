//使用： 
// vargrind --tool=memcheck ./test/testMemPool
#include <iostream>
#include "memPool.h"

int idx;
memoryPool<int,4> memp;
memoryPool<int>::Pointer a[10240];

int main (int argc, char *argv[]) {
    for(int i =1;i<=1025;i++)
    {
        a[i] = memp.get();
    }
    std::cout << memp.tot_new() << "\n";
    for(int i =1025;i>=1;i--) {
        memp.del(a[i]);
    }
    return 0;
}
