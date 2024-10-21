//存testPointResult的容器

#pragma once
#include <mutex>
#include <memory>
#include <functional>
#include <bits/stdc++.h>
#include <exception>

#include "judgeInfo.h"
#include "memPool.h"


class resultContainer {

public:
    using Pointer = typename memoryPool<testPointResult>::Pointer;

    struct node {
        Pointer head;
        int data_size;//数据的大小
        int finish_cnt;
        // struct testProblem problem;
    };

    resultContainer(int size){
        vec_.reserve(size+5);
        for(auto &p : vec_)
            p.head = nullptr;
    }

    //对p对应的testBoxId 里进行测试结束计数
    bool finish_cnt(Pointer p) {
        std::lock_guard lck(mtx_);
        int id = p->testBoxId;
        if( ++vec_[id].finish_cnt >= vec_[id].data_size)
            return true;
        return false;
    }

    ~resultContainer() {
        std::lock_guard lck(mtx_);
        for(auto &p : vec_)
        {
            while(p.head != nullptr) {
                auto t = p.head;
                p.head = p.head -> nxt;
                mem_.del(p.head);
            }
        }
    }

    //返回头部地址
    Pointer init_by_test_id(int testId,int data_size) {
        std::lock_guard lck(mtx_);
        vec_[testId].data_size = data_size;
        vec_[testId].finish_cnt = 0 ;
        if( vec_[testId].head != nullptr)
            throw std::runtime_error("init_by_test_id vec[testId].head not null");

        //一口气申请所有的需要写结果的内存
        for(int i = 0 ;i< data_size;i++) {
            auto t = mem_.get();
            t->nxt = vec_[testId].head;
            vec_[testId].head = t;
        }

        return vec_[testId].head;
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


    //移除所有的以idx为标记的所有的内存
    //放回memPool里
    void remove(int idx){
        std::lock_guard lck(mtx_);
        Pointer p = vec_[idx].head;
        vec_[idx].finish_cnt = 0;
        vec_[idx].data_size = 0;
        while(p != nullptr) {
            auto t = p;
            p = p -> nxt;
            mem_.del(t);
        }
    }



private:
    std::mutex mtx_;
    memoryPool<testPointResult> mem_; //内存池
    std::vector<node> vec_;

};
