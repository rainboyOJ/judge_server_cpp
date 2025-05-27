//队列的实现 
#include <mutex>
#include <vector>
#include <queue>
#include <memory>

template<typename T>
class UniquePtrQueue {
public:
    using Ptr = std::unique_ptr<T>;
    UniquePtrQueue() :size_(0)
    {}

    Ptr pop() {
        if( que_.empty())
            return nullptr;
        Ptr val = std::move(que_.front());
        que_.pop();
        return val;
    }

    void push(Ptr val) {
        que_.push(std::move(val));
    }

    bool empty() const {
        return que_.empty();
    }

    int size() {
        return que_.size();
    }

private:
    int size_;
    std::queue<Ptr> que_; // 存储 unique_ptr 的标准队列

};

