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
#include "memPool.h" //内存池,从中取出testPoint


/**
 * 实现单个点的测试
 * 内部有一个队列
 * 
 * 原理: 取出一个testPoint, 然后进行评测, 评测完成后, 通知testBox, 然后testBox再通知main线程, 通知main线程进行下一个测试点的测试
 * 
 * 原理: 多个工作线程,每一次push进数据后,都会notify_one,然后从队列中取出数据进行评测,评测完成后,会notify_one,然后通知main线程进行下一个测试点的测试
 *
 */
class testBox;


class testPointBox {

public:

    //单点的评测
    // using valPtr = UniquePtrQueue<testPoint>::Ptr;

    testPointBox(int Boolsize,testBox *);
    ~testPointBox();
    
    //添加测试点
    void push_one(testPoint * ptr);

    //添加多个评测点链表,注意这里这些是raw指针,是从memPool中申请的
    void push_link(testPoint * ptr);

    // 得到元素为size个的测试点testPoint链表
    testPoint * get_testPoint_link(int size);

private:
    void work();
    //评测工作
    void test(const testPoint * val);

private:
    // 需要评测的队列,从这里取出信息然后进行评测
    // UniquePtrQueue<testPoint> que_; 


    //队列
    testPoint * que_head;
    testPoint * que_tail;

    bool que_empty() const {
        return que_head == nullptr;
    }

    // 添加元素到队列,可能添加的本身就是一个链表
    void push_que(testPoint * ptr) {
        if( que_tail == nullptr ) { // 队列是空的
            que_head = ptr;
            que_tail = ptr;
        }
        else {
            que_tail->nxt = ptr;
        }

        // 移动到尾指针
        while( que_tail->nxt!= nullptr ){
            que_tail = que_tail->nxt;
        }
    }

    //取出一个元素
    testPoint * pop_que() {
        if( que_head == nullptr )
            return nullptr;

        testPoint * ptr = que_head;
        que_head = que_head->nxt;
        if( que_head == nullptr ) { // 队列只有一个元素
            que_tail = nullptr;
        }
        return ptr;
    }
    

    // testCompleteCallback testCompleteCallback_;

    // 线程池中的线程数量
    std::vector<std::thread> workers_;
    std::condition_variable cond_;
    std::mutex work_mtx;

    testBox * testBox_; //指向上一层的testBox

    // 用于停止线程池
    bool stop;

    // TODO 添加一个memoryPool, 
    // 1. get testPoint
    // 2. del testPoint

    // threadPool 线程池 work

    // 内存池
    memoryPool<testPoint> memPool_;
    std::mutex memPool_mtx_; // 内存池锁
};
