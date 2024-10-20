#pragma once
#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>

#include "judgeInfo.h"
#include "testQueue.h"


/**
 * 实现单个点的测试
 * 内部有一个队列
 *
 */
class testBox;


class testPointBox {

public:

    using valPtr = UniquePtrQueue<testPoint>::Ptr;

    testPointBox(int Boolsize,testBox *);
    ~testPointBox();
    
    //添加测试点
    void push(valPtr ptr);

private:
    void work();
    //评测工作
    void test(const testPoint * val);

private:
    UniquePtrQueue<testPoint> que_; //队列
    // testCompleteCallback testCompleteCallback_;

    // 线程池中的线程数量
    std::vector<std::thread> workers_;
    std::condition_variable cond_;
    std::mutex work_mtx;

    testBox * testBox_; //指向上一层的testBox

    // 用于停止线程池
    bool stop;

    // threadPool 线程池 work

};
