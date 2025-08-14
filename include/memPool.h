//块状内存池
#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>

// size 表示一次申请的内存块大小
template<typename T,int size=1024>
class memoryPool {

public:
    //保证数据的大小至少可以存下一个指针
    union TT {
        T v1;
        TT * nxt;
    };
    using Pointer = T*;

    T* get(){
        if( head == nullptr)
        {
            tot_new_ += size;
            head = new TT[size];
            mem_.push_back(head);
            for(int i = 0 ;i< size;i++) {
                head[i].nxt = &head[i+1];
            }
            head[size-1].nxt = nullptr;
        }
        TT * x = head;
        head = head -> nxt;
        return reinterpret_cast<T*>(x);
        // return static_cast<T*>(x);
        // return Pointer( reinterpret_cast<T*>(x),[this](T* p){this->del(p);});
    }
    unsigned long long tot_new() const {
        return tot_new_;
    }

    void del(T * p) {
        if(p == nullptr)
            throw std::runtime_error("memoryPool::del: p is nullptr");
        // TT * x = static_cast<TT*>(p);
        TT * x = reinterpret_cast<TT*>(p);
        x -> nxt = head;
        head = x;
    }

    ~memoryPool() {
        //统一删除内存块
        for(auto & pointer : mem_)
        {
            delete[] pointer;
        }
    }

private:
    TT * head = nullptr;
    std::vector<TT*> mem_;
    unsigned long long tot_new_ = 0;
};
