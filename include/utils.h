//执行一个程序
#pragma once

#include <array>
#include <vector>
#include <cstdio>  // For popen and pclose
#include <memory>  // For std::unique_ptr
#include <stdexcept>
#include <string>

std::string Popen(const char* cmd);


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


