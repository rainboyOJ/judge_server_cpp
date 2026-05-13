/**
 * @file utils.cpp
 * @brief 工具函数实现。
 */
#include "common/utils.h"
#include "common/Logger.h"

#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string_view>

std::string run_command_capture_stdout(const char *command) {
    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

static bool has_suffix(std::string const &value, std::string_view const &suffix) {
    if (value.length() >= suffix.length()) {
        return (0 == value.compare(value.length() - suffix.length(), suffix.length(), suffix));
    } else {
        return false;
    }
}

TestDataFilePairs scan_test_data_file_pairs(const fs::path &directory_path) {
    TestDataFilePairs file_pairs;

    using namespace std::literals;
    try {
        for (const auto& entry : fs::directory_iterator(directory_path)) {
            if (entry.is_regular_file()) {
                std::string file_name = entry.path().filename().string();

                if (has_suffix(file_name, ".in"sv)) {
                    std::string output_file_name = file_name;
                    output_file_name.replace(output_file_name.end() - 3, output_file_name.end(), ".out");
                    if (fs::exists(entry.path().parent_path() / output_file_name)) {
                        file_pairs.emplace_back(file_name, output_file_name);
                    }
                }
            }
        }

        for (const auto& pair : file_pairs) {
            std::cout << pair.first << " <-> " << pair.second << std::endl;
        }

    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return file_pairs;
}
