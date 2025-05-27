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

enum class readResultStatus {
    NOT_DATA, // 没有数据
    NOT_NEW_DATA, // 没有新的数据
    SUCCESS, // 成功读取数据
    FINISHED, // 所有结果都已经读取完毕
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

    resultContainer(int size){
        // 包含 std::mutex 的类型不能被复制，因此不能直接使用 vec_.resize 。
        // 又因为 testResult 没有默认构造函数，因此不能直接使用 vec_.resize(size); vec_.emplace_back();
        // 正确的做法是使用 std::vector 的构造函数，如下：

        vec_ = std::vector<testResult>(size + 5);
        for (auto &p : vec_)
          p.trp = nullptr;
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
     */
    void writeResult(int testBoxId,int test_point_seq_id,Pointer p);
        

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
        std::lock_guard lck(mtx_);
        for(auto &p : vec_)
        {
            while(p.test_problem_p != nullptr) {
                auto t = p.trp;
                p.trp = p.trp-> nxt;
                mem_.del(t);
            }
        }
    }

    //返回头部地址
    Pointer init_by_test_id(int testId,int data_size) {
        std::lock_guard lck(mtx_);
        vec_[testId].data_size = data_size; // 修改对应位置信息
        vec_[testId].finish_cnt = 0 ;
        vec_[testId].readDone_cnt = 0 ;
        if( vec_[testId].trp != nullptr)
            throw std::runtime_error("init_by_test_id vec[testId].head not null");

        //一口气申请所有的需要写结果的内存
        for(int i = 0 ;i< data_size;i++) {
            auto t = mem_.get();
            t->nxt = vec_[testId].trp;
            vec_[testId].trp= t;
        }

        return vec_[testId].trp;
    }

    //得到一个数据内存
    // Pointer get(int id) {
    //     std::lock_guard lck(mtx_);
    //     Pointer t = mem_.get();
    //     t -> testBoxId = id;
    //
    //     //加入数据
    //     t -> nxt = vec_[id].head;
    //     vec_[id].head = t;
    //
    //     return t;
    // }

    // 得到idx 对应的数据
    // 从resultContainer_ 读取数据,并返回 testResultWithVecotr 序列化后的数据
    std::vector<uint8_t> readResult(int idx,readResultStatus & status);


    //移除所有的以idx为标记的所有的内存
    //放回memPool里
    // TODO 改成更好的名字
    void clear(int idx){
        std::lock_guard lck(mtx_);
        Pointer p = vec_[idx].trp;
        vec_[idx].finish_cnt = 0;
        vec_[idx].data_size = 0;
        while(p != nullptr) {
            auto t = p;
            p = p -> nxt;
            mem_.del(t);
        }
    }

    //把testProblem 放入到对应的testBoxId 里
    void push_testProblem(int testBoxId,std::unique_ptr<testProblem> test_problem)
    {
        std::lock_guard lck(mtx_);
        vec_[testBoxId].test_problem_p = std::move(test_problem);
    }



private:
    std::mutex mtx_;
    memoryPool<testPointResult> mem_; //内存池
    std::vector<testResult> vec_;
    // vec_ 维护一个testBoxId 对应的 testPointResult 链表

/*
通过testBoxId(也就是下标) 找到对应的testPointResult 链表的头部

+----------+
+          +
+ infoHead + ---> testPointResult1 -> testPointResult2 -> ... -> testPointResultN
+          +
+----------+

+----------+
+          +
+ infoHead + ---> testPointResult1 -> testPointResult2 -> ... -> testPointResultN
+          +
+----------+


*/

};
