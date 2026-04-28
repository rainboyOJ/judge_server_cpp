/**
 * @file utils.cpp
 * @brief 工具函数实现。
 */
#include "utils.h"
#include "common/Logger.h"

#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string_view>

std::string Popen(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

static bool hasEnding(std::string const &fullString, std::string_view const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

Data_list_t scan_data_list(const fs::path &directoryPath) {
    Data_list_t filePairs;

    using namespace std::literals;
    try {
        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                std::string fileName = entry.path().filename().string();

                if (hasEnding(fileName, ".in"sv)) {
                    std::string outFileName = fileName;
                    outFileName.replace(outFileName.end() - 3, outFileName.end(), ".out");
                    if (fs::exists(entry.path().parent_path() / outFileName)) {
                        filePairs.emplace_back(fileName, outFileName);
                    }
                }
            }
        }

        for (const auto& pair : filePairs) {
            std::cout << pair.first << " <-> " << pair.second << std::endl;
        }

    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return filePairs;
}
