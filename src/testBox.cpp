#include "testBox.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;
using namespace std::literals;


bool hasEnding (std::string const &fullString, std::string_view const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}



testBox_err testBox::add(
        // char id_[32],
    std::string id_,
    std::string pid,
    std::string code,
    language lang
) {
    std::lock_guard lck(mtx_);
    // 得到一个loca id
    int lock_id = -1;
    if( !head_.empty()) {
        lock_id = head_.top();
        head_.pop();
    }
    else {
        return testBox_err::BUSY; //失败
    }
    // 创建一个valPtr
    // pValPtr testProblem = std::unique_ptr<struct testProblem>(new struct testProblem);

    // 转成testPoint

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
    for (const auto& pair : filePairs) {
        std::cout << pair.first << " <-> " << pair.second << std::endl;
        //添加到testPoint 里

        testPoint * t = new testPoint;
        t->seq_id = ++test_point_idx;
        // t->id_ = "123";
        // strcpy(t->id_,"123");
        pointBox_->push(std::unique_ptr<testPoint>(t));
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
