#pragma once
#include <vector>
//内存池
template<typename T,int size=1024>
class memoryPool {

public:
    //保证数据的大小至少可以存下一个指针
    union TT {
        T v1;
        TT * nxt;
    };

    T * get() {
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
        // return static_cast<T*>(x);
        return reinterpret_cast<T*>(x);
    }
    unsigned long long tot_new() const {
        return tot_new_;
    }

    void del(T * p) {
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
