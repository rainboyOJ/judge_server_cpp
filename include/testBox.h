// testPointBox的容器
// 接收一个题目的评测
#pragma once
#include "testPointBox.h"
#include "minheap.hpp"
#include <functional>
#include <filesystem>

namespace fs = std::filesystem;

enum class testBox_err {
    SUCC,
    PROBLEM_NOT_EXIST,
    DATA_NOT_EXIST,
    COMPILE_FAIL,
    BUSY,
};

using Data_list_t = std::vector< std::pair<std::string,std::string> >;


class testBox {
public:
    using beginTestCallback = std::function<void(const testProblem *)>;
    using singPointCompleteCallback = std::function<void(const testPointResult *)>;
    using allPointCompleteCallback = std::function<void(int)>;

    using pValPtr = UniquePtrQueue<testProblem>::Ptr;

    // dataSizeLimit 可以同时处理的数据的数量
    testBox(int workNum,int dataSizeLimit,
            const std::string & problem_path_
            )
    :
        head_(dataSizeLimit) ,
        problem_path(problem_path_),
        pointBox_(std::make_unique<testPointBox>(workNum))
    {
        head_.init();
    }

    ~testBox() {

    }

    //添加一个测试某个题目的测试数据

    testBox_err add(
        // char id_[32],
        std::string id_, //唯一评测id
        std::string pid,
        std::string code,
        language lang
    );


private:

    //处理testProblem 信息,转成 testPoint信息,传递给testPointBox
    void test_problem_info_deal(const testProblem *);

    Data_list_t scan_data_list(const fs::path & data_path);

    singPointCompleteCallback singPointCompleteCallback_;
    allPointCompleteCallback allPointCompleteCallback_;

    UniquePtrQueue<testProblem> que_; // 感觉不需要这个队列
    std::unique_ptr<testPointBox > pointBox_ = nullptr;
    MinHeap head_;

    const fs::path problem_path; //题目路径

    std::mutex mtx_; //锁


    // 一个存信息的数据池
    // TODO
};

