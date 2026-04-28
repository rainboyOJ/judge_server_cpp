/**
 * @file Config.h
 * @brief 全局单例配置对象，负责读取 config.json 并提供默认值安全的访问接口。
 */
#pragma once

#include <string>
#include <vector>
#include <iostream> // 为了使用std::cerr
#include <mutex> // 如果需要线程安全访问，虽然通常配置只读取一次
#include "json.hpp"

using json = nlohmann::json;

class Config {
public:
    static Config& getInstance();

    // 从JSON文件加载配置
    bool loadFromFile(const std::string& configFilepath);

    // 配置值的访问方法
    // 服务器设置
    std::string getServerHost() const;
    int getServerPort() const;
    int getConnectionTimeoutMs() const;

    // 测试设置
    int getWorkerThreadCount() const;
    int getMaxConcurrentTests() const;
    std::string getTestDataPath() const;
    int getResultRetentionSeconds() const;
    int getMaxStoredResults() const;

    // 性能设置
    size_t getBufferSize() const;

    // 日志设置
    std::string getLogFilePath() const;
    std::string getLogLevel() const; // 返回字符串如"INFO", "DEBUG"

private:
    Config(); // 单例模式的私有构造函数
    ~Config() = default;
    Config(const Config&) = delete;            // 删除拷贝构造函数
    Config& operator=(const Config&) = delete; // 删除赋值操作符

    json config_data_; // 保存解析后的JSON配置
    mutable std::mutex config_mutex_; // 保护对config_data_的访问，如果运行时可以修改的话
                                     // 或者如果多个线程可能调用loadFromFile（虽然对配置来说不太可能）

    // 安全获取值的辅助函数，提供默认值
    template<typename T> T get(const std::string& key, const T& defaultValue) const;
    template<typename T> T get(const json& subObject, const std::string& key, const T& defaultValue) const;
};

// 模板实现需要在头文件中或包含的.tpp文件中
// 从主config_data_对象获取值的辅助函数
template<typename T>
T Config::get(const std::string& key, const T& defaultValue) const {
    std::lock_guard<std::mutex> lock(config_mutex_); // 如果配置可能改变，确保线程安全读取
    try {
        if (config_data_.contains(key)) {
            return config_data_.at(key).get<T>();
        }
    } catch (const json::exception& e) {
        // 使用您的Logger适当地记录此错误
        // 现在只是用cerr，但理想情况下使用Logger::getInstance().error(...)
        std::cerr << "配置访问错误，键 '" << key << "': " << e.what() << std::endl;
    }
    return defaultValue;
}

// 从特定JSON子对象获取值的辅助函数
template<typename T>
T Config::get(const json& subObject, const std::string& key, const T& defaultValue) const {
    // 如果subObject是从锁定上下文传递的副本或const引用，这里不需要锁定config_mutex_
    try {
        if (subObject.contains(key)) {
            return subObject.at(key).get<T>();
        }
    } catch (const json::exception& e) {
        std::cerr << "子对象配置访问错误，键 '" << key << "': " << e.what() << std::endl;
    }
    return defaultValue;
}
