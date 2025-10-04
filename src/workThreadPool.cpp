#include "workThreadPool.h"
#include <mutex>
#include <cstddef> // offsetof
#include "common/Logger.h"

template <typename T>
void workThreadPoolQueue<T>::MemPoolDeleter::operator()(T* p) const noexcept {
    if (p == nullptr || owner == nullptr) return;
    // 从 T 指针恢复出包含它的 NewT 节点地址
    // 由于 NewT 的首成员是 T t; 确保与内存布局对齐
    auto* node = reinterpret_cast<typename workThreadPoolQueue<T>::NewT*>(
        reinterpret_cast<char*>(p) - offsetof(typename workThreadPoolQueue<T>::NewT, t)
    );
    std::lock_guard<std::mutex> lock(owner->memPool_mtx_);
    owner->memPool_.del(node);
}

template <typename T>
void workThreadPoolQueue<T>::push(const T &t) {
    // 由外层统一用 work_mtx 保证队列安全；这里仅保护内存池
    NewT *new_node = nullptr;
    {
        std::lock_guard<std::mutex> lock(memPool_mtx_);
        new_node = memPool_.get();
    }
    new_node->t = t;
    new_node->nxt = nullptr;

    if (que_tail == nullptr) {
        que_head = que_tail = new_node;
    } else {
        que_tail->nxt = new_node;
        que_tail = new_node;
    }
}

template <typename T>
typename workThreadPoolQueue<T>::Ptr workThreadPoolQueue<T>::pop() {
    // 由外层统一用 work_mtx 保证队列安全
    if (que_head == nullptr) {
        return nullptr;
    }

    NewT *node = que_head;
    que_head = que_head->nxt;
    if (que_head == nullptr) {
        que_tail = nullptr;
    }

    T* payload = &node->t;
    return Ptr(payload, MemPoolDeleter{this});
}

// 显式实例化常用模板
template class workThreadPoolQueue<PoolNode>;

// ===================== workThreadPool 实现 =====================

workThreadPool::workThreadPool(int Boolsize, resultContainer *resultContainerPtr)
    : resultContainerPtr_(resultContainerPtr), stop(false) {
    if (Boolsize <= 0) {
        Boolsize = 1;
    }
    workers_.reserve(static_cast<size_t>(Boolsize));
    for (int i = 0; i < Boolsize; ++i) {
        workers_.emplace_back(&workThreadPool::work, this);
    }
}

workThreadPool::~workThreadPool() {
    {
        std::lock_guard<std::mutex> lock(work_mtx);
        stop = true;
    }
    cond_.notify_all();
    for (auto &th : workers_) {
        if (th.joinable()) th.join();
    }
}

void workThreadPool::submitCompileAndTest(int testBoxId) {
    PoolNode node{};
    node.testBoxId = testBoxId;
    node.testStage = TestStage::PRE_DEAL;
    node.seq_id = 0;
    {
        // 用 work_mtx 保护队列
        std::lock_guard<std::mutex> lk(work_mtx);
        task_queue_.push(node);
    }
    cond_.notify_one();
}

// ============ 包装 task_queue_ 的线程安全操作 ============
void workThreadPool::push_task(const PoolNode &task) {
    std::lock_guard<std::mutex> lk(work_mtx);
    task_queue_.push(task);
}

auto workThreadPool::pop_task() -> workThreadPoolQueue<PoolNode>::Ptr {
    std::lock_guard<std::mutex> lk(work_mtx);
    return task_queue_.pop();
}

void workThreadPool::work() {
    for (;;) {
        LOG_DEBUG("workThreadPool::work() start");
       
        workThreadPoolQueue<PoolNode>::Ptr  taskPtr  = nullptr;
        // 等待任务
        {
            std::unique_lock<std::mutex> lock(work_mtx);
            // cond_.wait(lock, [this] { return stop || !task_queue_.que_empty(); });
            while( !stop && task_queue_.que_empty() /* 进入等待的条件 */  )
            {
                cond_.wait(lock); // 释放锁
            }
            // 结束条件
            if (stop && task_queue_.que_empty()) {
                return;
            }

            // 这里为什么不用 pop_stask() ?
            // 因为 这里已经得到了锁
            taskPtr = task_queue_.pop();
        }

        if (!taskPtr) {
            // 可能被其他线程抢先取走，继续等待
            continue;
        }

        PoolNode task = *taskPtr;

        // 简单的阶段流转：PRE_DEAL -> COMPILE -> TEST
        switch (task.testStage) {
            case TestStage::PRE_DEAL: {
                bool ok = PreDeal(task.testBoxId, resultContainerPtr_);
                if (ok) {
                    PoolNode nxt{task.testBoxId, TestStage::COMPILE, 0};
                    push_task(nxt);
                    cond_.notify_one();
                } else {
                    // 预处理失败，可根据需要记录错误（此处保守处理）
                }
                break;
            }
            case TestStage::COMPILE: {
                bool ok = Compile(task.testBoxId,resultContainerPtr_);
                if (ok) {
                    PoolNode nxt{task.testBoxId, TestStage::TEST, 0};
                    push_task(nxt);
                    cond_.notify_one();
                } else {
                    // 编译失败，保守处理
                }
                break;
            }
            case TestStage::TEST: {
                // 这里的简单实现只做一次单点测试
                (void)TestOneSinglePoint(task.testBoxId,resultContainerPtr_);
                break;
            }
            default:
                break;
        }
    }
}



bool Compile() {
    return  true;
}

bool TestOneSinglePoint() {
    return  true;
}