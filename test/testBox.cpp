#include <cstring>
#include <iostream>
#include <chrono>
#include <filesystem>
namespace fs = std::filesystem;

#include "testBox.h"

char id_[] = "1000";

int main (int argc, char *argv[]) {
    //创建一个testPointBox
    std::string problem_path ;
    try {
        // 获取当前程序的路径
        fs::path currentPath = fs::current_path();

        // 获取当前路径的上一层目录
        fs::path parentPath = currentPath.parent_path();

        problem_path = (parentPath / "testData").string();

    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    std::cout << "problem_path = " << problem_path << "\n";
    testBox TB(3,20, problem_path);
    auto res = TB.add(
        "1000-rainboy",
        "1000",
        "code",
        language::cpp
    );
    if( res != testBox_err::SUCC)
    {
        std::cout << "testBox add Fail!" << "\n";
    }
    else {
        std::cout << "testBox add SUCC!" << "\n";
    }
    // 添加任务
    return 0;
}
