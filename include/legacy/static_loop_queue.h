//一个简单的由数组实现的循环队列

// 设计,有N
// 

template <typename T>
class StaticLoopQueue
{
public:
    StaticLoopQueue(int N) : head_(0), tail_(0) ,arr_size(N+1) {
        //申请内存
        a = new T[arr_size];
    }

    ~StaticLoopQueue() {
        delete[] a;
    }

    // 为了能存N个元素这里的数组大小是N+1,
    // 因为tail_指向尾部元素的下一个位置,这里保证tail_指向的位置一定是空
    // 所以tail_ == head_表示队列为空,
    // head_ = (tail_ + 1) % (N+1)表示队列满,而满的时候,已经存下了N个元素,

    bool empty() {
        return head_ == tail_; //head_ 指向了一个空位置
    }

    //队列是否满
    bool full() {
        return (tail_ + 1) % arr_size == head_;
    }

    //加入元素
    bool push(T t) {
        if( full() )
            return false;
        a[tail_++] = t;
        tail_ %= arr_size;
        return true;
    }

    T pop() {
        if( empty() )
            throw std::runtime_error("pop from empty queue");
        T t = a[head_++];
        head_ %= arr_size;
        return t;
    }

private:
    const int arr_size;
    T * a;
    int head_;
    int tail_;
};