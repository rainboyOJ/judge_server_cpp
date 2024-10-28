// 一些评测需要用到的信息结构体
#pragma once
#include <cstdint>
#include <string>
#include <vector>
//评测时需要的一些信息


//评测结果
struct judge_result {
    int cpu_time; // millisecond
    int real_time; //millisecond
    long memory; // kb
    int signal; // 退出的信号
    int exit_code; // 程序的退出值
    int error; // 错误值
    int result; // 结果 0
};

enum language {
    cpp,
    c,
    python
};

//一个题目的评测

struct testProblem {
    int uuid; // 唯一评测id
    char pid[8]; //题目的id
    language lang; // 语言
    std::string code; //代码
};


struct testPointResult {
    int testBoxId; // 唯一评测TextBoxId
    int seq_id; // 评测的测试点编号

    int cpu_time; // ms
    int real_time; // ms
    unsigned long long  memory; //kb
    int signal;
    int exit_code;
    int error;
    int result;
    testPointResult * nxt;

    // char msg[128]; //评测结果的信息 或 错误信息
};

//传递给testPointBox,进行一个一个的评测
// 为了能进行内存管理,这里必须使用 trivial 类型的结构体
using char_str = char[128];
struct  testPoint
{
    int testBoxId; // 唯一评测TestBoxId,用于获得在testBox 中需要存结果的位置
    // char pid[8]; //题目的id, // 应该不需要pid吧
    testPointResult * testPointResult_p; //存测试结果的内存地址

    int seq_id; // 评测的测试点编号
    language lang; // 语言
    int cpu_time; // 时间限制
    int real_time;
    unsigned long long memory; //内存限制

    char_str exe; //评测程序的路径
    char_str cwd; //运行路径
    char_str input_path; // 输入数据
    char_str output_path; // 正确的输出数据

    testPoint * nxt; //下一个测试点的指针
    // 这里是为了可以一次性的把所有测试点信息写入传递给PointBox中
};

// 测试点的错误类型
enum testError {
    succ = 0, // 正确
    prob_not_found, // 题目不存在
    data_not_found, // 数据不存在
    compile_error, // 编译错误
    other, // 其他错误
};

//测试结果
struct testResult {
    int uuid; // 唯一评测id
    int testBoxId; //唯一主评测id // 对应于testBox 中使用
    // int seq_id; // 评测的测试点编号
    testError err_type; // 错误类型
    language lang;
    testPointResult * trp; //存测试的内存地址,这是一个链表的开头
    // std::string msg; //评测信息
    // std::string err; //评测错误信息
    // judge_result result_;
};

//测试结果 vecotr 版本
struct testResultWithVecotr {
    int uuid; // 唯一评测id
    int testBoxId; //唯一主评测id // 对应于testBox 中使用
    testError err_type; // 错误类型
    language lang;
    std::vector<testPointResult> trp; //只是这里改成了vecotr
};


//序列化与反序列化
// 我不想用 google 的 protobuf 库, 太重,如果我想到实现内存管理还要实现 Arena 机制, 那就太麻烦了.
//我里直接使用最粗暴的方式,手动写一个大暴力


// 对接收的测试信息testProblem进行序列化与反序列化
std::vector<uint8_t> serializeTestProblem(const testProblem &tp);
void deserializeTestProblem(const uint8_t * s, testProblem &tp);

// 对发送的测试结果testPointResult进行序列化与反序列化
std::vector<uint8_t> serializeTestPointResult(const testResult &tpr);
void deserializeTestPointResult(const uint8_t * s, testResultWithVecotr &tpr);

