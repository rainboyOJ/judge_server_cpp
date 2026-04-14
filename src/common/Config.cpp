#include "common/Config.h" // 调整后的路径
#include "common/Logger.h" // 用于在配置加载期间记录错误
#include <fstream>
#include <stdexcept> // 用于std::runtime_error

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

Config::Config() {
    // 用默认值初始化或保持空直到调用loadFromFile
    // 如果配置文件是可选的，在这里设置一些合理的默认值是好习惯
    // 例如：
    // config_data_["server"]["host"] = "0.0.0.0";
    // config_data_["server"]["port"] = 8080;
    // config_data_["logging"]["level"] = "INFO";
}

bool Config::loadFromFile(const std::string& configFilepath) {
    std::lock_guard<std::mutex> lock(config_mutex_); // 保护config_data_
    std::ifstream ifs(configFilepath);
    if (!ifs.is_open()) {
        LOG_ERROR("Failed to open config file: %s", configFilepath.c_str());
        return false;
    }

    try {
        ifs >> config_data_;
        LOG_INFO("Configuration loaded successfully from: %s", configFilepath.c_str());
        return true;
    } catch (const json::exception& e) {
        config_data_ = json{}; // 出错时重置为空或默认值
        LOG_ERROR("Failed to parse config file '%s': %s", configFilepath.c_str(), e.what());
        return false;
    }
}

// 访问器实现
std::string Config::getServerHost() const {
    const json server = config_data_.contains("server") && config_data_["server"].is_object()
                            ? config_data_["server"]
                            : json::object();
    return get<std::string>(server, "host", "0.0.0.0");
}

int Config::getServerPort() const {
    const json server = config_data_.contains("server") && config_data_["server"].is_object()
                            ? config_data_["server"]
                            : json::object();
    return get<int>(server, "port", 8080);
}

int Config::getConnectionTimeoutMs() const {
    const json server = config_data_.contains("server") && config_data_["server"].is_object()
                            ? config_data_["server"]
                            : json::object();
    return get<int>(server, "connection_timeout_ms", 30000);
}

int Config::getWorkerThreadCount() const {
    const json testing = config_data_.contains("testing") && config_data_["testing"].is_object()
                             ? config_data_["testing"]
                             : json::object();
    return get<int>(testing, "worker_thread_count", 4);
}

int Config::getMaxConcurrentTests() const {
    const json testing = config_data_.contains("testing") && config_data_["testing"].is_object()
                             ? config_data_["testing"]
                             : json::object();
    return get<int>(testing, "max_concurrent_tests", 4);
}

std::string Config::getTestDataPath() const {
    const json testing = config_data_.contains("testing") && config_data_["testing"].is_object()
                             ? config_data_["testing"]
                             : json::object();
    return get<std::string>(testing, "test_data_path", "./testData");
}

int Config::getResultRetentionSeconds() const {
    const json testing = config_data_.contains("testing") && config_data_["testing"].is_object()
                             ? config_data_["testing"]
                             : json::object();
    return get<int>(testing, "result_retention_seconds", 600);
}

int Config::getMaxStoredResults() const {
    const json testing = config_data_.contains("testing") && config_data_["testing"].is_object()
                             ? config_data_["testing"]
                             : json::object();
    return get<int>(testing, "max_stored_results", 1000);
}

size_t Config::getBufferSize() const {
    const json performance = config_data_.contains("performance") && config_data_["performance"].is_object()
                                 ? config_data_["performance"]
                                 : json::object();
    return get<size_t>(performance, "buffer_size", 8192);
}

std::string Config::getLogFilePath() const {
    const json logging = config_data_.contains("logging") && config_data_["logging"].is_object()
                             ? config_data_["logging"]
                             : json::object();
    return get<std::string>(logging, "file_path", "application.log");
}

std::string Config::getLogLevel() const {
    const json logging = config_data_.contains("logging") && config_data_["logging"].is_object()
                             ? config_data_["logging"]
                             : json::object();
    return get<std::string>(logging, "level", "INFO");
}
