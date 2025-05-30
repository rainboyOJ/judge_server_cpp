#include "common/Config.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

void Config::loadFromFile(const std::string& config_file) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            std::cerr << "Warning: Cannot open config file: " << config_file 
                      << ", using default values" << std::endl;
            return;
        }
        
        json config_json;
        file >> config_json;
        
        // 加载服务器配置
        if (config_json.contains("server")) {
            const auto& server = config_json["server"];
            if (server.contains("port")) {
                server_port_ = server["port"].get<int>();
            }
            if (server.contains("connection_timeout_ms")) {
                connection_timeout_ms_ = server["connection_timeout_ms"].get<int>();
            }
        }
        
        // 加载测试配置
        if (config_json.contains("testing")) {
            const auto& testing = config_json["testing"];
            if (testing.contains("worker_thread_count")) {
                worker_thread_count_ = testing["worker_thread_count"].get<int>();
            }
            if (testing.contains("max_concurrent_tests")) {
                max_concurrent_tests_ = testing["max_concurrent_tests"].get<int>();
            }
            if (testing.contains("test_data_path")) {
                test_data_path_ = testing["test_data_path"].get<std::string>();
            }
        }
        
        // 加载性能配置
        if (config_json.contains("performance")) {
            const auto& performance = config_json["performance"];
            if (performance.contains("buffer_size")) {
                buffer_size_ = performance["buffer_size"].get<size_t>();
            }
        }
        
        std::cout << "Config loaded successfully from: " << config_file << std::endl;
        std::cout << "Server port: " << server_port_ << std::endl;
        std::cout << "Worker threads: " << worker_thread_count_ << std::endl;
        std::cout << "Max concurrent tests: " << max_concurrent_tests_ << std::endl;
        std::cout << "Test data path: " << test_data_path_ << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading config file " << config_file 
                  << ": " << e.what() << ", using default values" << std::endl;
    }
}