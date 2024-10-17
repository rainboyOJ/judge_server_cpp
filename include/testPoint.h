//单个点的测试信息
#pragma once
#include "judgeInfo.h"
#include <string>
#include <vector>

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

// helper
std::string get_problem_path_from_pid(char * pid);

//通过路径,得到下面的数据列表
std::vector<std::pair<std::string,std::string>> get_data_list_from_path(char * pid);


//得到评测时需要的评测参数
// uint32_t    max_cpu_time{UNLIMITED}; // seconds
// uint32_t    max_real_time{UNLIMITED}; // seconds
// uint64_t    max_memory{UNLIMITED}; // mb
// uint64_t    max_stack{UNLIMITED}; //mb
void get_judge_args_from_path(int&);


