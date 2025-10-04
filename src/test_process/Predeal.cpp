#include <string_view>


#include "workThreadPool.h"
#include "common/Logger.h"
#include "testBox.h"
#include "utils.h"
using namespace std::literals;


bool PreDeal(const int testBoxId, resultContainer *resultContainerPtr){
    LOG_DEBUG("PreDeal Start, testBoxId %d", testBoxId);

    // 扫描数据
    // pid type : char [32];
    auto pid = resultContainerPtr->getPid(testBoxId);
    LOG_DEBUG("pid: %s", pid);

    auto testBoxPtr = resultContainerPtr->getTestBoxPtr();

    auto problem_path = testBoxPtr->getProbelmPath();
    fs::path pid_path = problem_path / pid;
    if( !fs::exists(pid_path)){
        LOG_DEBUG("pid_path: %s not exist\n", pid_path.c_str());
        //TODO : add error message to resultContainer
        return false;
    }
    

    fs::path data_path = pid_path / "data";
    if( !fs::exists(data_path)){
        //TODO : add error message to resultContainer
        return false;
    }
    LOG_DEBUG("pid_path: %s\n", pid_path.c_str());
    LOG_DEBUG("data_path: %s\n", data_path.c_str());

    Data_list_t filePairs= scan_data_list(data_path);

    // 把数据加入到 resultContainer_ 里
    int seq_id = 0;
    const std::string work_dir = "/tmp/" + std::to_string(testBoxId);
    if( !fs::exists(work_dir))
        fs::create_directory(work_dir);
    else
        // 清空目录
        fs::remove_all(work_dir);
    for (const auto &pair : filePairs)
    {
        // std::cout << pair.first << " <-> " << pair.second << std::endl;
        // 这里的pair.first 是输入数据的文件名, pair.second 是输出数据的文件名
        LOG_DEBUG("pair.first: %s, pair.second: %s", pair.first.c_str(), pair.second.c_str());

        TestCaseInfo info;
        // TODO 这里的信息应该从题目配置文件里读取 resultContainer.problem 里面
        info.testBoxId = testBoxId;
        info.seq_id = ++seq_id; //  TODO seq_id 应该从 文件名里读取
        info.cpu_time_limit = 1000; // ms
        info.real_time_limit = 1000 * 10; // ms
        info.memory_limit = 1024 * 1024; //kb
        strcpy(info.exe, "main");
        strcpy(info.cwd, work_dir.c_str());
        strcpy(info.input_path, (data_path / pair.first).c_str());
        strcpy(info.output_path, (data_path / pair.second).c_str());
        strcpy(info.user_output_path, (data_path / ("user_"s + pair.second)).c_str());

        resultContainerPtr->setTestCaseInfo(testBoxId, info.seq_id, info);
    }

    // 结束 preDeal
    return true;


    // // 代码编译
    // //TODO

    // // 输出结果
    // int test_point_idx = 0;

    // 初始化
    // resultContainer_.init_by_test_id(testBoxId, filePairs.size());

    // 连续申请 n 个内存
    /*
    // TODO 删除这里,使用新的方式来添加评测数据
    testPointResult  * head =  resultContainer_.allocateTestPointResult_of_N(filePairs.size());

    // 得到 一串 testPoint * 内存的链表
    testPoint *testPointList = pointBox_ -> get_testPoint_link(filePairs.size());
    testPoint *t = testPointList;
    {
        // TODO 这里 应该不要加锁,因为这里只是对已经申请好的内存进行操作
        // std::lock_guard lck(mtx_); // 加锁 
        for (const auto &pair : filePairs)
        {
            // std::cout << pair.first << " <-> " << pair.second << std::endl;
            // 这里的pair.first 是输入数据的文件名, pair.second 是输出数据的文件名
            LOG_DEBUG("pair.first: %s, pair.second: %s\n", pair.first.c_str(), pair.second.c_str());


            t->seq_id = ++test_point_idx; //编号
            t->testBoxId = testBoxId;
            // !! 这里改写评测结果写入的内存地址
            t->testPointResult_p = head; //结果应该写入的内存
            t = t->nxt;
            head = head->nxt;
        }
        // [!!流程3!!] 数据进入testPointBox ,
        // resultContainer_.move_in_testProblem(testBoxId, std::move(test_problem));
        resultContainer_.push_testProblem(testBoxId, std::move(test_problem));
        pointBox_->push_link(testPointList); // 把链表放入队列,等待评测,一次评测多个点
    }
    */

    // return TestBoxVoidResult::success({});

    return true;
}