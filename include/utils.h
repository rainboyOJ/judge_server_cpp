//执行一个程序
#pragma once

#include <array>
#include <cstdio>  // For popen and pclose
#include <memory>  // For std::unique_ptr
#include <stdexcept>
#include <string>

std::string Popen(const char* cmd);


//评测
std::string judge(
    long long max_cpu_time,
    long long max_real_time,
    long long max_memory,
    std::string exe,
    std::string input,
    std::string output
);

// 根据字符串得到它的结果
