// testPointBox的容器
// 接收一个题目的评测
#pragma once
#include <functional>
#include <filesystem>
#include "testPointBox.h"
// #include "minheap.hpp" //不再使用这个heap, 因为用队列更快
#include "static_loop_queue.h"
#include "resultContainer.h"

/*
testBox设计的思路:

1. testBoxId [1,2,3,4....]

使用步骤:
0. 构造函数传入workNum, dataSizeLimit, problem_path
1. 设置回调函数 singPointCompleteCallback, allPointCompleteCallback ,当对应的评测事件发生时,会调用对应的回调函数,传递一个参数testBoxId
    // 什么使用使用回回调函数的方式,不使用传递评测结果的方式呢? 主要是上一层为 client_sockets ,起到了一个 debounce 的作用,避免过多的评测结果传递到 client_sockets 上,造成网络堵塞
2. getTestBoxId() 得到一个可以用的空位置, 并返回对就的int值
3. 调用add() 传入testBoxId, 评测信息, 评测结果
4. 等待回调的发生
*/


namespace fs = std::filesystem;

enum class testBox_err {
    SUCC,
    PROBLEM_NOT_EXIST,
    DATA_NOT_EXIST,
    COMPILE_FAIL,
    BUSY,
};


//题目的测试数据的名字列表
using Data_list_t = std::vector< std::pair<std::string,std::string> >;

using singPointCompleteCallback = std::function<void(const testPointResult *)>;
using allPointCompleteCallback = std::function<void(int)>;

class testBox {
public:

    friend class testPointBox;

    using beginTestCallback = std::function<void(const testProblem *)>;
    using singPointCompleteCallback = std::function<void(const testPointResult *)>;
    using allPointCompleteCallback = std::function<void(int)>;

    using pValPtr = UniquePtrQueue<testProblem>::Ptr;

    // dataSizeLimit 可以同时处理的数据的数量
    testBox(int workNum,int dataSizeLimit,
            const std::string & problem_path_
            )
    :
        dataSizeLimit_(dataSizeLimit),
        testBoxIdQue_(std::make_unique<StaticLoopQueue<int>>(dataSizeLimit) ) ,
        problem_path(problem_path_),
        pointBox_(std::make_unique<testPointBox>(workNum,this)),
        resultContainer_(dataSizeLimit) //设计可以现时进行的测试的题目的多少
    {
        //在队列里存n个元素
        for (int i = 0;i< dataSizeLimit_;i++)
        {
            testBoxIdQue_->push(i);
        }
    }

    int size() const { return dataSizeLimit_; }

    void setSingPointCompleteCallback(singPointCompleteCallback callback);
    void setallPointCompleteCallback();

    //处理 testPointBox 调用过来的信息

    ~testBox() {

    }

    //添加一个测试某个题目的测试数据

    //得到一个可以用的空位置,-1表示没有空位置了
    int getTestBoxId();
    // 放回testBoxId
    void putBackTestBoxId(int id);


    //得到结果,返回的为protobuf格式的字符串
    std::string getResult(const int testBoxId);

    //清空
    // testBox_err add(
    //     int testBox_id, // testBox 可以用的空位置
    //     // char id_[32],
    //     const int uuid, //唯一评测id
    //     const std::string pid,
    //     const std::string code,
    //     language lang
    // );

    //添加一个评测
    // TODO
    // 这里不应该有testBox_id,因为由内部分配,外部看不到这个东西,
    // 根据返回值来确定是否添加成功
    testBox_err add(
        const int testBox_id, // testBox 可以用的空位置
        std::unique_ptr<testProblem> test_problem
    );

private:

    //处理testPointBox传递过来的评测结果
    void deal_testPoint_singlePointComplete(testPointResult *);

    //处理testProblem 信息,转成 testPoint信息,传递给testPointBox
    void test_problem_info_deal(const testProblem *);

    Data_list_t scan_data_list(const fs::path & data_path);

    singPointCompleteCallback singPointCompleteCallback_;
    allPointCompleteCallback allPointCompleteCallback_;

    UniquePtrQueue<testProblem> que_; // 感觉不需要这个队列
    std::unique_ptr<testPointBox > pointBox_ = nullptr;

    // MinHeap heap_;
    
    //存有可用testBoxId的队列
    std::unique_ptr<StaticLoopQueue<int>> testBoxIdQue_ = nullptr;

    const fs::path problem_path; //题目路径

    std::mutex mtx_; //锁

    resultContainer resultContainer_;
    const int dataSizeLimit_; //同时处理的数据的数量


    // 一个存信息的数据池
    // TODO
};

