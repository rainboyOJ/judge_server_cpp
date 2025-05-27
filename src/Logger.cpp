#include "Logger.h"
#include "Timestamp.h"
#include <iostream>
#include <string>
#include <string_view>

Logger &Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level) { logLevel_ = level; }

void Logger::log(const char * _file, const int _line, std::string msg) {
    std::string pre = "";
    switch (logLevel_) {
        case INFO:
            pre = "[INFO] ";
            break;
        case ERROR:
            pre = "[ERROR] ";
            break;
        case FATAL:
            pre = "[FATAL] ";
            break;
        case DEBUG:
            pre = "[DEBUG] ";
            break;
        default:
            break;
    }

    // 处理文件路径，去除工程目录前缀
    std::string file_path(_file);
    
    // 使用CMake定义的项目根目录
    #ifdef PROJECT_ROOT_DIR
    const std::string_view project_prefix = PROJECT_ROOT_DIR;
    #else
    const std::string project_prefix = "";
    #endif
    
    // 查找并移除项目前缀
    if (!project_prefix.empty()) {
        size_t prefix_pos = file_path.find(project_prefix);
        if (prefix_pos != std::string::npos) {
            file_path = file_path.substr(prefix_pos + project_prefix.length());
        }
    }
    
    //!NOTE: 将日志打印由两次改为一次，避免并发时打印错位
    std::cout 
        << " [" << file_path << ":" << std::dec << _line << "]"
        << pre 
        << Timestamp::now().toString() 
        << ": " << msg << std::endl;
}
