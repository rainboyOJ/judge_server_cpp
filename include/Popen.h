//执行一个程序
#pragma once

#include <array>
#include <cstdio>  // For popen and pclose
#include <memory>  // For std::unique_ptr
#include <stdexcept>
#include <string>

std::string Popen(const char* cmd);
