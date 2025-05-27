#pragma once

#include <utility>

/**
 * 实现最小堆的功能
 *
 */



class MinHeap {
public:
    MinHeap(const int size) :size_(size), cur_(0)
    {
        //申请内存
        mem_ = new int[size_+5];
    }

    //初化1到n的数据,堆
    void init() {
        for(int i= 1;i<=size_;++i) 
            mem_[i] = i;
        cur_ =  size_;
    }

    ~MinHeap() {
        delete[] mem_;
    }

    int top() const { return mem_[1]; }

    bool empty() const { return cur_ == 0;}

    void pop() {
        std::swap(mem_[1],mem_[cur_]);
        --cur_;
        pushdown();
    }

    void add(int v) {
        if( cur_ >= size_) return ;
        mem_[++cur_] = v;
        pushup();
    }

private:
    inline int left_ch(int v) { return v<< 1;}
    inline int right_ch(int v) { return (v<< 1)|1;}
    bool exist(int idx) { return idx <=cur_ && idx >= 1; }

    //向下调整
    // 和左右存在的最小孩子进行交换
    void pushdown() {
        int now = 1;

        while(1) {
            int nxt = left_ch(now);
            int r = right_ch(now);

            if( ! exist(nxt)) break; //不存在，就跳出
            if( exist(r) && mem_[r] < mem_[nxt]  )
                nxt = r;
            if( mem_[now] > mem_[nxt]) {
                std::swap(mem_[nxt],mem_[now]);
                now = nxt;
            }
            else break;
        }

    }

    //向上调整
    void pushup() {
        int now = cur_;

        while(1) {
            int fa = now >> 1;
            if( fa == 0) break;//不存在，就跳出

            if(mem_[fa] > mem_[now]  )
            {
                std::swap(mem_[fa],mem_[now]);
                now = fa;
            }
            else break;
        }

    }


private:
    int * mem_ = nullptr;
    const int size_; //内存的容量
    int cur_; //当前的末尾下标

};
