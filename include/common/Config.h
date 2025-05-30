#pragma once
#include <string>
#include <memory>
#include "json.hpp"

using json = nlohmann::json;

class Config {
public:
    static Config& getInstance() {
        static Config instance;
        return instance;
    }
    
    // 网络配置
    int getServerPort() const { return server_port_; }
    
    // 测试配置
    int getWorkerThreadCount() const { return worker_thread_count_; }
    int getMaxConcurrentTests() const { return max_concurrent_tests_; }
    std::string getTestDataPath() const { return test_data_path_; }
    
    // 资源配置
    size_t getBufferSize() const { return buffer_size_; }
    int getConnectionTimeout() const { return connection_timeout_ms_; }
    
    // 加载配置文件
    void loadFromFile(const std::string& config_file);
    
    // 设置方法（用于测试或运行时修改）
    void setServerPort(int port) { server_port_ = port; }
    void setWorkerThreadCount(int count) { worker_thread_count_ = count; }
    void setMaxConcurrentTests(int count) { max_concurrent_tests_ = count; }
    void setTestDataPath(const std::string& path) { test_data_path_ = path; }
    void setBufferSize(size_t size) { buffer_size_ = size; }
    void setConnectionTimeout(int timeout) { connection_timeout_ms_ = timeout; }
    
private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    // 默认配置值
    int server_port_ = 8000;
    int worker_thread_count_ = 4;
    int max_concurrent_tests_ = 4;
    std::string test_data_path_ = "./testData";
    size_t buffer_size_ = 8192;
    int connection_timeout_ms_ = 30000;
};