#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <string_view>
#include <utility>

#include "testBox.h"
#include "Logger.h"


namespace fs = std::filesystem;
using namespace std::literals;


bool hasEnding (std::string const &fullString, std::string_view const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

int testBox::getTestBoxId() {
    int testBoxId = -1;
    {
        std::lock_guard lck(mtx_);
        if( !testBoxIdQue_ -> empty() )
            testBoxId = testBoxIdQue_->pop();
    }
    return testBoxId;
}

void testBox::putBackTestBoxId(int id) {
    std::lock_guard lck(mtx_);
    //因为放的元素都是从队列中取出来的,所以这里不用担心会超过队列的大小
    testBoxIdQue_ -> push(id); 
}

testBox_err testBox::add(
    const int testBoxId,
    std::unique_ptr<testProblem> test_problem)
{
    // getpid
    const std::string pid = test_problem->pid;

    fs::path pid_path = this->problem_path / pid;
    if( !fs::exists(pid_path))
        return testBox_err::PROBLEM_NOT_EXIST;

    fs::path data_path = pid_path / "data"sv;
    if( !fs::exists(data_path))
        return testBox_err::DATA_NOT_EXIST;

    Data_list_t filePairs= scan_data_list(data_path);

    // 代码编译
    //TODO

    // 输出结果
    int test_point_idx = 0;

    // 得到 [存放的结果] 头部指针,注意这里一次就分配好了所有需要的存n结果的链表的内存
    testPointResult *head = resultContainer_.init_by_test_id(testBoxId, filePairs.size());

    // 得到 一串 testPoint * 内存的链表
    testPoint *testPointList = pointBox_ -> get_testPoint_link(filePairs.size());
    testPoint *t = testPointList;
    {
        // TODO 这里 应该不要加锁,因为这里只是对已经申请好的内存进行操作
        // std::lock_guard lck(mtx_); // 加锁 
        for (const auto &pair : filePairs)
        {
            std::cout << pair.first << " <-> " << pair.second << std::endl;
            // TODO 得到pair.first 对应的 编号
            // 添加到testPoint 里

            t->seq_id = ++test_point_idx; //编号
            t->testBoxId = testBoxId;
            // !! 这里改写评测结果写入的内存地址
            t->testPointResult_p = head; //结果应该写入的内存

            t = t->nxt;
            head = head->nxt; // 下一个可以写入结果的地址
            // t->id_ = "123";
            // strcpy(t->id_,"123");
            // 加入到 testPointBox 里
            
            //TODO pointBox_ 一次添加多个元素
            // pointBox_->push(std::unique_ptr<testPoint>(t));
        }
        pointBox_->push_link(testPointList);
    }

    return testBox_err::SUCC;
}

void testBox::test_problem_info_deal(const testProblem *tp_) {

}



Data_list_t
    testBox::scan_data_list(const fs::path & directoryPath){
    Data_list_t filePairs;
    // const std::string directoryPath = "/path/to/your/directory"; // 替换为你的目录路径

    using namespace std::literals;
    try {
        // 遍历指定目录
        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                std::string fileName = entry.path().filename().string();

                // 检查文件扩展名
                // if (fileName.ends_with(".in")) {
                if (hasEnding(fileName,".in"sv)) {
                    std::string outFileName = fileName;
                    outFileName.replace(outFileName.end() - 3, outFileName.end(), ".out"); // 替换 .in 为 .out
                    // 检查对应的 .out 文件是否存在
                    if (fs::exists(entry.path().parent_path() / outFileName)) {
                        filePairs.emplace_back(fileName, outFileName);
                    }
                }
            }
        }

        // 输出结果
        for (const auto& pair : filePairs) {
            std::cout << pair.first << " <-> " << pair.second << std::endl;
        }

    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return filePairs;
}




void testBox::setSingPointCompleteCallback(singPointCompleteCallback callback)
{
    this -> singPointCompleteCallback_ = callback;
}

void testBox::deal_testPoint_singlePointComplete(testPointResult * resultPtr) {
    std::lock_guard lck(mtx_);
    LOG_INFO("--> after parse_test_point_result \n\
            result %d \n\
            signal %d \n\
            exit_code %d \n\
            error %d \n\
            cpu_time %d \n\
            real_time %d \n\
            memory %lld \n\
            \n",
             resultPtr->result,
             resultPtr->signal,
             resultPtr->exit_code,
             resultPtr->error,
             resultPtr->cpu_time,
             resultPtr->real_time,
             resultPtr->memory
             );

    // 处理信息
    int testBoxId = resultPtr-> testBoxId;

    bool finish = resultContainer_.finish_cnt(resultPtr); // 加1
    if( finish ) {

        // TODO 把结果链 转成对应的信息
        // !!! 信息传递给对应的 消息队列，

        //把结果链 删除(放回内存池子)
        // 不应该在这里发删除,应该在main线程发送完数据后删除
        // resultContainer_.remove(testBoxId);
    }
    LOG_INFO("Finsh %d\n",finish);
}


std::string getResult(const int testBoxId) {
    // std::lock_guard lck(mtx_);
    // TODO


}
