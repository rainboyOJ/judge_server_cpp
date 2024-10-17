//内存池
template<typename T>
class memoryPool {

    //保证数据的大小至少可以存下一个指针
    union TT {
        T v1;
        TT * nxt;
    };

    T * get() {
        if( head == nullptr)
        {
            return new TT;
        }
        TT * x = head;
        head = head -> nxt;
        return x;
    }

    void push(T * p) {
        TT * x = static_cast<TT*>(p);
        x -> nxt = head;
        head = x;
    }

    ~memoryPool() {
        while(head) {
            TT * x = head;
            head = head->nxt;
            delete x;
        }
    }

private:
    TT * head = nullptr;
};
