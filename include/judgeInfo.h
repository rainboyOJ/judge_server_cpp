// 一些评测需要用到的信息结构体
#pragma once
#include <string>
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

struct testPoint {
    int judge_id; //唯一主评测id // 对应于testBox 中使用
    char id_[32]; // 唯一评测id
    int seq_id; // 评测的测试点编号
    language lang;
    // std::string msg; //评测信息
    // std::string err; //评测错误信息
    // judge_result result_;
};

//一个题目的评测

struct testProblem {
    char id_[32]; // 唯一评测id
    char pid[32]; //题目的id
    language lang; // 语言
    std::string code; //代码

    int judge_id; //唯一主评测id // 对应于testBox 中使用
};

struct testPointResult {
    int signal;
    std::string msg; //评测结果的信息
    std::string err; //错误信息
    // std::string msg; //评测信息
    // std::string err; //评测错误信息
    // judge_result result_;
    int judge_id; //唯一主评测id // 对应于testBox 中使用
    char id_[32]; // 唯一评测id
    int seq_id; // 评测的测试点编号
};


//评测
std::string judge(
    long long max_cpu_time,
    long long max_real_time,
    long long max_memory,
    std::string exe,
    std::string input,
    std::string output
);
