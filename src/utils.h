/**
 * @file utils.h
 * @brief 工具函数声明。
 */
#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

std::string Popen(const char* cmd);

using Data_list_t = std::vector<std::pair<std::string, std::string>>;
Data_list_t scan_data_list(const fs::path &directoryPath);
