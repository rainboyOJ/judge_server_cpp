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

std::string run_command_capture_stdout(const char *command);

using TestDataFilePairs = std::vector<std::pair<std::string, std::string>>;
TestDataFilePairs scan_test_data_file_pairs(const fs::path &directory_path);
