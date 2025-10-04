#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "judgeInfo.h"
#include "memPool.h" //内存池,从中取出testPoint
#include "resultContainer.h"
// #include "testQueue.h"  // 这个队列不再使用

/**
 * 实现单个点的测试
 * 内部有一个队列
 *
 * 原理: 取出一个testPoint, 然后进行评测, 评测完成后, 通知testBox,
 * 然后testBox再通知main线程, 通知main线程进行下一个测试点的测试
 *
 * 原理:
 * 多个工作线程,每一次push进数据后,都会notify_one,然后从队列中取出数据进行评测,评测完成后,会notify_one,然后通知main线程进行下一个测试点的测试
 *
 * 框架:
 *  1. task_queue: 用于存储需要评测的测试点
 *  2. result_container: 用于存储评测结果
 *  3. memoryPool: 内存池
 */
// class testBox;

// 函数声明: 预处理阶段,对测试点进行预处理,比如读取数据,读取配置文件等
enum class TestStage {
    PRE_DEAL,
    COMPILE,
    TEST,
};

bool PreDeal(const int testBoxId, resultContainer *resultContainerPtr);
// 函数声明: 编译阶段
bool Compile(const int testBoxId, resultContainer *resultContainerPtr);
// 函数声明: 评测阶段,评测一个单独的测试点
bool TestOneSinglePoint(const int testBoxId, const resultContainer *resultContainerPtr);

// 一个队列,用于存储需要评测的数据,workThreadPool会从这里取数据,根据返回值,
//  进行不同的行为 1. compile 2. test
struct PoolNode {
    int testBoxId; // 唯一评测TestBoxId,用来获取resultContainer中存储的相关信息
    TestStage testStage; // 测试阶段
    int seq_id;          // 测试点编号
};

template <typename T = PoolNode> class workThreadPoolQueue {
  public:
    // 队列,保证所有的操作都是在有锁 work_mtx 的情况下进行的

    struct NewT {
        T t;
        NewT *nxt;
    };

    // 使用自定义删除器, 在 unique_ptr 生命周期结束时归还到内存池
    struct MemPoolDeleter {
        workThreadPoolQueue<T> *owner;
        void operator()(T *p) const noexcept;
    };

    using Ptr = std::unique_ptr<T, MemPoolDeleter>;

    bool que_empty() const { return que_head == nullptr; }
    void push(const T &t);
    Ptr pop();

  private:
    NewT *que_head = nullptr;
    NewT *que_tail = nullptr;
    // 内存池
    memoryPool<NewT> memPool_;
    std::mutex memPool_mtx_; // 仅保护内存池分配/释放
};

class workThreadPool {

  public:
    // 单点的评测
    //  using valPtr = UniquePtrQueue<testPoint>::Ptr;

    workThreadPool(int Boolsize, resultContainer *resultContainerPtr);
    ~workThreadPool();

    // 加入队列里面一个 TestStage = PRE_DEAL ,让work线程去执行编译和测试
    void submitCompileAndTest(int testBoxId);

  private:
    // Wrap task_queue_

    void push_task(const PoolNode &task);

    auto pop_task() -> workThreadPoolQueue<PoolNode>::Ptr;

  private:
    // 工作线程执行的函数
    void work();

    // 需要评测的队列,从这里取出信息然后进行评测
    // UniquePtrQueue<testPoint> que_;

    // 任务队列：驱动编译/测试阶段（基于 PoolNode）
    workThreadPoolQueue<PoolNode> task_queue_;

    // testCompleteCallback testCompleteCallback_;

    // 线程池中的线程数量
    std::vector<std::thread> workers_;
    std::condition_variable cond_;
    std::mutex work_mtx;

    resultContainer *resultContainerPtr_; // 指向resultContainer

    // 用于停止线程池
    bool stop;
};
