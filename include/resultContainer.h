//存testPointResult的容器

#pragma once
#include <vector>
#include <mutex>
#include <memory>
#include <functional>
#include <bits/stdc++.h>
#include <exception>

#include "judgeInfo.h"
#include "memPool.h"
#include "utils.h"
#include "json.hpp"
using json = nlohmann::json;

enum class readResultStatus {
    NOT_DATA, // 没有数据
    NOT_NEW_DATA, // 没有新的数据
    SUCCESS, // 成功读取数据
    FINISHED, // 所有结果都已经读取完毕
};

using TestPointResultMemoryPool = memoryPool<testPointResult>;
/**
 * @brief 自定义删除器函数，用于释放 `testPointResult` 对象的内存。
 * 
 * 该函数接收一个指向 `TestPointResultMemoryPool` 内存池的指针和一个指向 `testPointResult` 对象的指针。
 * 如果 `testPointResult` 指针不为空，它会调用内存池的 `del` 方法将该对象的内存归还给内存池。
 * 
 * @param pool 指向 `TestPointResultMemoryPool` 内存池的指针，用于管理 `testPointResult` 对象的内存。
 * @param p 指向 `testPointResult` 对象的指针，该对象的内存将被释放。
 */
inline void __deleter(TestPointResultMemoryPool * pool,testPointResult * p) {
    // 检查指针是否为空，如果不为空则调用内存池的 del 方法释放内存
    if(p != nullptr)
        pool->del(p); // 释放内存
}

using TestPointResultPtr = std::unique_ptr<testPointResult, std::function<void(testPointResult*)>>;

inline TestPointResultPtr create_testPointResult(TestPointResultMemoryPool * pool, testPointResult * resultMem_) {
    // 创建一个 unique_ptr，使用自定义删除器函数 __deleter
    // 这个 unique_ptr 会在超出作用域时自动调用 __deleter 来释放内存
    return std::unique_ptr<testPointResult, std::function<void(testPointResult*)>>(resultMem_, [pool](testPointResult* p) {
        __deleter(pool, p);
    });
}

//测试结果
class testResult {
public:
    int testBoxId; //唯一主评测id // 对应于testBox 中使用
    int uuid; // 唯一评测id
    testError err_type; // 错误类型

    std::unique_ptr<testProblem> test_problem_p; // 题目信息
    std::vector<TestPointResultPtr> TPR; // 存储测试点结果的指针数组,每个元素是一个 testPointResult 的智能指针

    int data_size; // 数据点的数量
    int finish_cnt;
    int readDone_cnt;

    std::mutex mtx_; // 互斥锁

    // 不在兼容旧代码
    // testPointResult * trp;

public:
    testResult()
        : testBoxId(-1),
          uuid(-1),
          err_type(testError::succ),
          data_size(0),
          finish_cnt(0), readDone_cnt(0),
          TPR(100) // 初始化 TPR 数组大小为 100
    {
        // 初始化互斥锁
    }

    // 添加测试点结果
    bool addTestPointResult(int idx,TestPointResultPtr && trp) {
        std::lock_guard<std::mutex> lock(mtx_);
        TPR[idx] = std::move(trp);
        ++finish_cnt;
        return finish_cnt >= data_size; // 返回是否所有测试点都已完成
    }


    // 序列化测试结果
    json serialize() const;

    // 清空并初始化测试数据
    void clear();

    void init_by_test_id(int testBoxId, int data_size) {
        std::lock_guard<std::mutex> lock(mtx_);
        this->testBoxId = testBoxId;
        this->data_size = data_size;
        this->finish_cnt = 0;
        this->readDone_cnt = 0;
        // this->TPR.resize(data_size); // 调整 TPR 数组大小
        for (auto &trp : TPR)
        {
            if (trp)
            {
                // 调用自定义删除器释放内存
                trp.reset();
            }
        }
    }
};

class resultContainer {

public:
    // 这个Pointer 就是 testPointResult*
    using Pointer = typename memoryPool<testPointResult>::Pointer;

    // struct node {
        // Pointer head;
        // int data_size;//数据的大小
        // int finish_cnt;
        // // struct testProblem problem;
        // std::shared_ptr<testProblem> test_problem; //指向测试题目
    // };

    resultContainer(int size) : mem_() {
        // 包含 std::mutex 的类型不能被复制，因此不能直接使用 vec_.resize 。
        // 又因为 testResult 没有默认构造函数，因此不能直接使用 vec_.resize(size); vec_.emplace_back();
        // 正确的做法是使用 std::vector 的构造函数，如下：

        vec_ = std::vector<testResult>(size + 5);
        // 初始化每个testResult的基本信息
        for (int i = 0; i < vec_.size(); ++i) {
            vec_[i].testBoxId = i;
        }
    }

    //对p对应的testBoxId 里进行测试结束计数
    bool isJudgeFinished(int testBoxId) const{
        // return finish_cnt(p);
        return vec_[testBoxId].finish_cnt >= vec_[testBoxId].data_size;
    }

    /**
     * 向结果容器中写入一个测试点的评测结果
     * 
     * @param testBoxId         测试箱ID,用于标识不同的测试实例
     * @param test_point_seq_id 测试点序号,表示当前是第几个测试点
     * @param p                 指向测试点结果的指针,存储了该测试点的详细评测信息
     * @return bool            写入结果的状态
     *                        - true: 所有测试点都已完成评测
     *                        - false: 还有测试点未完成评测
     * 
     * @note 此函数是线程安全的,内部使用互斥锁保护共享资源
     * @note 写入成功后会增加完成计数器(finish_cnt)
     * @warning testBoxId必须是有效的索引值,且小于容器大小
     * @see isJudgeFinished() 检查评测是否完成
     */
    bool writeResult(int testBoxId, int test_point_seq_id, Pointer p) {
        if (testBoxId < 0 || testBoxId >= vec_.size()) return false;
        
        // std::lock_guard<std::mutex> lock(vec_[testBoxId].mtx_);
        auto & result = vec_[testBoxId];
        return result.addTestPointResult(test_point_seq_id, create_testPointResult(&mem_,p));
    }
        

// TODO change name -> add_finish_cnt 添加计数
// TODO 这里的锁是是不是 用 std::atomic_int 更好呢?
    // bool finish_cnt(Pointer p) {
    //     // TODO 这里的锁是是不是 用 auto_mitic<int> 更好呢?
    //     std::lock_guard lck(mtx_);
    //     int id = p->testBoxId;
    //     if( ++vec_[id].finish_cnt >= vec_[id].data_size)
    //         return true;
    //     return false;
    // }

    ~resultContainer() {
        std::lock_guard<std::mutex> lck(mtx_);
        for(auto &result : vec_) {
            for( auto & t : result.TPR) {
                if( t) t.reset();
            }
        }
    }

    //返回头部地址
    void init_by_test_id(int testId, int data_size) {
        if (testId < 0 || testId >= vec_.size()) {
            throw std::out_of_range("testId out of range");
        }

        // std::lock_guard<std::mutex> lck(mtx_);
        auto & result = vec_[testId];
        result.init_by_test_id(testId, data_size);

        // if (result.trp != nullptr)
        //     throw std::runtime_error("init_by_test_id vec[testId].trp not null");

        // 一口气申请所有的需要写结果的内存
        // for(int i = 0; i < data_size; i++) {
        //     auto t = mem_.get();
        //     t->testBoxId = testId;
        //     t->seq_id = i;
        //     t->nxt = result.trp;
        //     result.trp = t;
        // }

        // return result.trp;
    }

    //得到一个数据内存,
    TestPointResultPtr get(int testBoxId) {
        std::lock_guard lck(mtx_);
        return create_testPointResult(&mem_,mem_.get()); // 使用自定义删除器创建智能指针
    }

    /**
     * 清空指定测试箱的所有评测结果并释放相关资源
     * 
     * @param idx 测试箱索引(testBoxId),用于标识要清空的测试实例
     * 
     * @note 此函数是线程安全的,内部使用互斥锁保护共享资源
     * @note 清空操作包括:
     *       - 重置完成计数器(finish_cnt)为0
     *       - 重置数据大小(data_size)为0  
     *       - 遍历并释放该测试箱对应的所有testPointResult链表节点
     *       - 将释放的内存归还给内存池(memPool)
     * 
     * @warning idx必须是有效的索引值,且小于容器大小
     * 
     * TODO: 考虑重命名为更清晰的函数名,如clearTestResults()或resetTestBox()
     */
    void resetTestBoxById(int idx) {
        if (idx < 0 || idx >= vec_.size()) return;
        auto & result = vec_[idx];
        result.clear();
    }

    //把testProblem 放入到对应的testBoxId 里
    void push_testProblem(int testBoxId, std::unique_ptr<testProblem> test_problem) {
        if (testBoxId < 0 || testBoxId >= vec_.size()) return;

        std::lock_guard<std::mutex> lck(mtx_);
        vec_[testBoxId].test_problem_p = std::move(test_problem);
        vec_[testBoxId].uuid = vec_[testBoxId].test_problem_p->uuid;
    }

    /**
     * 设置错误类型
     */
    void setErrorType(int testBoxId, testError err_type) {
        if (testBoxId < 0 || testBoxId >= vec_.size()) return;

        std::lock_guard<std::mutex> lock(vec_[testBoxId].mtx_);
        vec_[testBoxId].err_type = err_type;
    }

    /**
     * 获取测试结果的引用（用于直接操作）
     */
    testResult& getTestResult(int testBoxId) {
        if (testBoxId < 0 || testBoxId >= vec_.size()) {
            throw std::out_of_range("testBoxId out of range");
        }
        return vec_[testBoxId];
    }

    /**
     * 读取JSON格式的测试结果
     */
    json readResultAsJson(int idx, readResultStatus & status) {
        if (idx < 0 || idx >= vec_.size()) {
            status = readResultStatus::NOT_DATA;
            return json{};
        }

        std::lock_guard<std::mutex> lock(vec_[idx].mtx_);
        auto & result = vec_[idx];

        if (result.finish_cnt == 0) {
            status = readResultStatus::NOT_DATA;
            return json{};
        }

        if (result.readDone_cnt >= result.finish_cnt) {
            status = readResultStatus::NOT_NEW_DATA;
            return json{};
        }

        result.readDone_cnt = result.finish_cnt;
        
        if (result.finish_cnt == result.data_size) {
            status = readResultStatus::FINISHED;
        } else {
            status = readResultStatus::SUCCESS;
        }

        return result.serialize();
    }
public:

    /**
     * @brief 从内存池中分配一个新的测试点结果对象
     * 
     * 该函数从内存池中获取一个 testPointResult 对象，并将其包装在智能指针中。
     * 智能指针使用自定义删除器，确保对象在超出作用域时自动归还给内存池。
     * 
     * @param testBoxId 测试箱ID，用于标识该测试点结果属于哪个测试实例
     *                  (注意：当前实现中此参数未被使用，但保留用于未来扩展)
     * 
     * @return TestPointResultPtr 指向新分配的 testPointResult 对象的智能指针
     *                           该智能指针会在析构时自动将内存归还给内存池
     * 
     * @note 此函数是线程安全的，使用互斥锁保护内存池的并发访问
     * @note 返回的智能指针使用 RAII 模式，无需手动释放内存
     * @note 如果内存池无法分配新对象，底层的 mem_.get() 可能返回 nullptr
     * 
     * @see create_testPointResult() 创建智能指针的工厂函数
     * @see TestPointResultPtr 测试点结果的智能指针类型别名
     * 
     * @example
     * auto result_ptr = container.allocateTestPointResult(testBoxId);
     * if (result_ptr) {
     *     result_ptr->seq_id = 1;
     *     result_ptr->cpu_time = 100;
     *     // 智能指针会自动管理内存
     * }
     */
    TestPointResultPtr allocateTestPointResult(int testBoxId) {
        std::lock_guard<std::mutex> lck(mtx_);
        return create_testPointResult(&mem_,mem_.get()); // 使用自定义删除器创建智能指针
    }

    // 连续申请 n 个内存
    testPointResult * allocateTestPointResult_of_N(int n) {
        std::lock_guard<std::mutex> lck(mtx_);
        testPointResult *head = nullptr;
        for (int i = 0; i < n; ++i)
        {
            testPointResult *  result = mem_.get();
            result -> nxt = head;
            head = result;
        }
        return head;
    }

private:
    std::mutex mtx_;
    memoryPool<testPointResult> mem_; // 内存池
    std::vector<testResult> vec_;     // 维护一个testBoxId 对应的 testResult 的数组
};
