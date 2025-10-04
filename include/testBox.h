// testPointBox的容器
// 接收一个题目的评测
#pragma once
#include <functional>
#include <filesystem>
#include <string_view>
#include <mutex>
#include <vector>
#include "testPointBox.h"
// #include "minheap.hpp" //不再使用这个heap, 因为用队列更快
#include "static_loop_queue.h"
#include "resultContainer.h"
#include "workThreadPool.h"
#include "common/Result.h"

#include "json.hpp"
using json = nlohmann::json;

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

// 使用新的Result类型替代原有的枚举错误码
// enum class testBox_err 已被 TestBoxError 和 Result<T, TestBoxError> 替代
using TestBoxError = std::string_view;
const std::string_view TestBoxError_PROBLEM_NOT_EXIST = "PROBLEM_NOT_EXIST";
const std::string_view TestBoxError_DATA_NOT_EXIST = "DATA_NOT_EXIST";
using TestBoxVoidResult = Result<Unit, TestBoxError>;

//题目的测试数据的名字列表
using Data_list_t = std::vector< std::pair<std::string,std::string> >;

using singPointCompleteCallback = std::function<void(const testPointResult *)>;
using allPointCompleteCallback = std::function<void(int)>;

class testBox {
public:

    friend class testPointBox;

    using beginTestCallback = std::function<void(int testBoxId)>;
    using singPointCompleteCallback = std::function<void(int testBoxId)>;
    using allPointCompleteCallback = std::function<void(int)>;

    using pValPtr = UniquePtrQueue<testProblem>::Ptr;

    // dataSizeLimit 可以同时处理的数据的数量,testBoxId 的数量
    testBox(int workNum,int dataSizeLimit,
            const std::string & problem_path_
            )
    :
        dataSizeLimit_(dataSizeLimit),
        testBoxIdQue_(std::make_unique<StaticLoopQueue<int>>(dataSizeLimit) ) ,
        problem_path(problem_path_),
        // pointBox_(std::make_unique<testPointBox>(workNum,this)),
        resultContainer_(dataSizeLimit,this), //设计可以现时进行的测试的题目的多少
        workThreadPool_(workNum, &resultContainer_),
        allPointCompleteCallback_(nullptr),
        singPointCompleteCallback_(nullptr)
    {
        //在队列里存n个元素
        for (int i = 0;i< dataSizeLimit_;i++)
        {
            testBoxIdQue_->push(i);
        }
    }

    // 最多可以同时测试多少个题目
    int size() const { return dataSizeLimit_; }

    void setSingPointCompleteCallback(singPointCompleteCallback callback) {
        this -> singPointCompleteCallback_ = callback;
    }
    void setallPointCompleteCallback(allPointCompleteCallback callback) {
        this -> allPointCompleteCallback_ = callback;
    }

    //处理 testPointBox 调用过来的信息

    ~testBox() {

    }

    //添加一个测试某个题目的测试数据


    // 更好的名字
    // int getTestBoxIdNoWorking();



    //得到结果,返回的为JSON格式的字符串
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
    bool add(
        int uuid,
        char pid[32],
        language lang,
        std::string&& code //右值引用: 避免不必要的拷贝，提高效率
    );


    //通过testBoxId 得到resultContainer_里的结果
    // 并返回testResult 序列化后的结果
    std::string getResultByTestBoxId(int testBoxId);

    void clearResultByTestBoxId(int testBoxId); //清空resultContainer_里的结果

protected:

    // 向 resultContainer 写入测试结果
    // @param testBoxId  测试箱ID,用于标识不同的测试实例
    // @param seq_id     测试点的序号
    // @param trp        测试点结果的指针,包含该测试点的详细评测信息
    void writeResult(int testBoxId,int seq_id,const  TestCaseResult & trp);

private:
    // 得到一个可以用的空位置,-1表示没有空位置了
    int getTestBoxId();
    // 放回testBoxId
    void putBackTestBoxId(int id);

    //处理testPointBox传递过来的评测结果
    void deal_testPoint_singlePointComplete(testPointResult *);

    //处理testProblem 信息,转成 testPoint信息,传递给testPointBox
    void test_problem_info_deal(const testProblem *);

    Data_list_t scan_data_list(const fs::path & data_path);

    singPointCompleteCallback singPointCompleteCallback_;
    allPointCompleteCallback allPointCompleteCallback_;

    UniquePtrQueue<testProblem> que_; // 感觉不需要这个队列
    // std::unique_ptr<testPointBox > pointBox_ = nullptr;

    //存有可用testBoxId的队列
    std::unique_ptr<StaticLoopQueue<int>> testBoxIdQue_ = nullptr;
    
    const fs::path problem_path; //题目路径

    std::mutex mtx_; //锁

    resultContainer resultContainer_;
    workThreadPool workThreadPool_;
    const int dataSizeLimit_; //同时处理的数据的数量


    // 一个存信息的数据池
    // TODO
};

