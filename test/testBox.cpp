#include <cstring>
#include <iostream>
#include <chrono>
#include <filesystem>
namespace fs = std::filesystem;

#include "testBox.h"

char id_[] = "1000";
int a[1000000+5];

int main (int argc, char *argv[]) {
    memset(a,0,sizeof(a));
    for(int i = 1;i <= 1000 ;++i ) // i: 1->1000
    {
        a[i] = i;
    }
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
    

    // 创建一个评测线程为4个, 评测队列大小为20,基础题目地址为problem_path的testBox
    testBox TB(4,20, problem_path);

    // int testBoxId =  TB.getTestBoxId();

    std::string code = "#include <iostream>\nint main() {\n\tstd::cout << \"Hello World!\" << std::endl;\n\treturn 0;\n}\n";

    auto res = TB.add(
        9527, // uuid
        id_, // problem id
        language::cpp,
        std::move(code) // 使用std::move将字符串转换为右值引用
    );
    if(  res == true)
    {
        std::cout << "testBox add Fail!" << "\n";
    }
    else {
        std::cout << "testBox add SUCC!" << "\n";
    }
    // 添加任务

    // TB.pushBackTestBoxId(testBoxId);
    // TB.putBackTestBoxId(testBoxId);
    return 0;
}
