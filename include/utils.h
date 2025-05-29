//执行一个程序
#pragma once

#include <array>
#include <vector>
#include <cstdio>  // For popen and pclose
#include <memory>  // For std::unique_ptr
#include <stdexcept>
#include <string>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include <type_traits>
#include <arpa/inet.h>
#include "judgeInfo.h"
#include "json.hpp"
using json = nlohmann::json;

// 前向声明
class testResult;

std::string Popen(const char* cmd) ;

//序列化与反序列化
// 我不想用 google 的 protobuf 库, 太重,如果我想到实现内存管理还要实现 Arena 机制, 那就太麻烦了.
//我里直接使用最粗暴的方式,手动写一个大暴力

//把整数 int 或long long 转换成字符串
template<typename T>
void serializeInt(const T &t,std::vector<uint8_t> &ret){
    int size = sizeof(T);
    if constexpr ( std::is_same<T, int>::value  || std::is_same<T, unsigned int>::value )
    {
        T x = htonl(t);
        for(int i=0; i<size; i++){
            ret.push_back( x & 0xff );
            x >>= 8;
        }
    }
    else if constexpr ( std::is_same<T, long long>::value  || std::is_same<T, unsigned long long>::value )
    {
        T x = htobe64(t);
        for(int i=0; i<size; i++){
            ret.push_back( x & 0xff );
            x >>= 8;
        }
    }
}

//转成整数 int 或long long
template<typename T>
T deserializeInt(const uint8_t * src){
    int size = sizeof(T);
    T x = 0;
    for (int i = size-1; i >=0; i--)
    {
        x <<= 8;
        x |= src[i];
    }
    if constexpr ( std::is_same<T, int>::value  || std::is_same<T, unsigned int>::value )
    {
        return ntohl(x);
    }
    else if constexpr ( std::is_same<T, long long>::value  || std::is_same<T, unsigned long long>::value )
    {
        return be64toh(x);
    }
}

// 对接收的测试信息testProblem进行序列化
std::vector<uint8_t> serializeTestProblem(const testProblem &tp);
void deserializeTestProblem(const uint8_t * s, testProblem &tp);

// JSON版本的序列化和反序列化函数
json serializeTestProblemToJson(const testProblem &tp);
std::unique_ptr<testProblem> deserializeTestProblemFromJson(const json &j);
std::unique_ptr<testProblem> deserializeTestProblemFromJsonString(const std::string &json_str);

// 对发送的测试结果testPointResult进行反序列化
std::vector<uint8_t> serializeTestPointResult(const testResult &tpr);

// 根据字符串得到它的结果


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


void parse_test_point_result(const std::string & str,testPointResult * result);

//评测函数
std::string judge(
    long long max_cpu_time,
    long long max_real_time,
    long long max_memory,
    std::string exe,
    std::string input,
    std::string output
);

// 反序列化测试点结果
void deserializeTestPointResult(const uint8_t* s, testResultWithVecotr &tpr);

// 调试用的函数,打印出uint8_t 数组
void debug_print_uint8_t_vector(const std::vector<uint8_t> &vec);