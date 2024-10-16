//测试点结果

#include <string>

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
