#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>


// 如何设计一个线程池,且这个线程池中的工作线程工作的方式都一样
// 且当这个

class ThreadPool {
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();

    // 添加一个任务到线程池
    template<typename F>
    auto enqueue(F&& f) -> std::future<typename std::result_of<F()>::type>;

private:
    // 工作线程运行的函数
    void worker();

    // 线程池中的线程数量
    std::vector<std::thread> workers;

    // 任务队列
    std::queue<std::function<void()>> tasks;

    // 互斥量和条件变量
    std::mutex queueMutex;
    std::condition_variable condition;

    // 用于停止线程池
    bool stop;
};

// 构造函数
ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back(&ThreadPool::worker, this); // 创建工作线程
    }
}

// 析构函数
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true; // 设置停止标志
    }
    condition.notify_all(); // 通知所有线程
    for (std::thread &worker : workers) {
        worker.join(); // 等待每个线程结束
    }
}

// 向线程池添加任务
template<typename F>
auto ThreadPool::enqueue(F&& f) -> std::future<typename std::result_of<F()>::type> {
    using return_type = typename std::result_of<F()>::type; // 获取返回类型

    auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f)); // 创建任务

    std::future<return_type> res = task->get_future(); // 获取返回值
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.emplace([task]() { (*task)(); }); // 将任务添加到队列
    }
    condition.notify_one(); // 通知一个等待的线程
    return res; // 返回 future
}

// 工作线程运行的函数
void ThreadPool::worker() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { return stop || !tasks.empty(); }); // 等待任务

            if (stop && tasks.empty()) {
                return; // 如果停止且队列为空，退出线程
            }

            task = std::move(tasks.front()); // 获取队列前端的任务
            tasks.pop(); // 从队列中移除任务
        }
        task(); // 执行任务
    }
}

// 测试线程池
int main() {
    ThreadPool pool(4); // 创建一个包含 4 个线程的线程池

    // 添加任务到线程池
    for (int i = 0; i < 10; ++i) {
        pool.enqueue([i] {
            std::cout << "Task " << i << " is running on thread " 
                      << std::this_thread::get_id() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟工作
        });
    }

    // 主线程等待一段时间
    std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0; // 程序结束时会自动调用析构函数，清理资源

}
