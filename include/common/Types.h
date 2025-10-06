#pragma once

#include <string>
#include <vector>
#include <cstdint> // 用于固定宽度整数如int32_t, uint64_t
#include <chrono>  // 用于时间相关类型

// 可能在应用程序中使用的基本类型定义。
// 这有助于维护一致性。

// 示例：如果经常使用ID，定义一个ID类型
using EntityId = uint64_t;

// 示例：如果需要为特定上下文定义特定的字符串类型
// （虽然std::string通常就够了）
// using SpecialStringType = std::string;

// 网络相关类型（如果变得很多，可以移动到网络特定的类型头文件中）
// using PortNumber = uint16_t;
// using IpAddress = std::string;

// 时间相关（通常直接使用std::chrono类型更好）
// using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

// 广泛使用的常量也可以放在这里，如果它们不适合
// 特定的类或模块上下文。
// 但是，在适当的地方更喜欢类枚举或静态const成员。

// namespace Constants {
//     constexpr int MAX_CONNECTIONS = 100;
//     constexpr double PI = 3.1415926535;
// }

// 如果您有跨模块使用的特定错误代码，可以在这里定义
// enum class GlobalErrorCode {
//     SUCCESS = 0,
//     UNKNOWN_ERROR = 1,
//     NETWORK_ERROR = 100,
//     FILE_IO_ERROR = 200
// };

// 这个文件旨在成为非常通用的类型和可能的常量的集合。
// 避免在这里放置复杂的类定义。保持简单。


//-------------- 评测相关类型 --------------

// TODO: 需要根据 simple_judge 的返回值修改
enum enum_testStatus {
    INIT = -1, // 初始状态,未开始评测
    AC = 0, // accept 通过
    WA,  // wrong answer 错误答案
    TLE, // time limit exceeded 超时
    MLE, // memory limit exceeded 内存超限
    RE, // runtime error 运行时错误
    OLE, // output limit exceeded 输出超限
    PE, // presentation error 格式错误
    CE, // compile error 编译错误
    UNKNOWN, // 未知错误
};

typedef char char_str[1024];

struct TestCaseInfo {
    int testBoxId; // 唯一评测TextBoxId
    int seq_id; // 评测的测试点编号
    int cpu_time_limit; // ms
    int real_time_limit; // ms
    int memory_limit; //kb

    char_str exe; //评测程序的路径
    char_str cwd; //运行路径
    char_str input_path; // 输入数据
    char_str output_path; // 正确的输出数据
    char_str user_output_path; // 用户的输出数据
};

// 一个测试点的结果
struct TestCaseResult {
    int testBoxId; // 唯一评测TextBoxId
    int seq_id; // 评测的测试点编号

    int cpu_time; // ms
    int real_time; // ms
    unsigned long long  memory; //kb
    int signal;
    int exit_code;
    int error;
    enum_testStatus result;
    TestCaseResult * nxt;

    // char msg[128]; //评测结果的信息 或 错误信息
};

//-------------- 评测相关类型 END --------------