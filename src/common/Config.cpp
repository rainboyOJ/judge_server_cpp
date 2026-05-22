/**
 * @file Config.cpp
 * @brief 运行时配置加载与安全访问实现。
 */
#include "common/Config.h" // 调整后的路径
#include "common/Logger.h" // 用于在配置加载期间记录错误
#include <fstream>
#include <stdexcept> // 用于std::runtime_error

namespace {

json configSection(const json &config_data, const std::string &name) {
  return config_data.contains(name) && config_data[name].is_object()
             ? config_data[name]
             : json::object();
}

} // namespace

Config &Config::getInstance() {
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

bool Config::loadFromFile(const std::string &configFilepath) {
  std::lock_guard<std::mutex> lock(config_mutex_); // 保护config_data_
  std::ifstream ifs(configFilepath);
  if (!ifs.is_open()) {
    LOG_ERROR("Failed to open config file: %s", configFilepath.c_str());
    return false;
  }

  try {
    ifs >> config_data_;
    LOG_INFO("Configuration loaded successfully from: %s",
             configFilepath.c_str());
    return true;
  } catch (const json::exception &e) {
    config_data_ = json{}; // 出错时重置为空或默认值
    LOG_ERROR("Failed to parse config file '%s': %s", configFilepath.c_str(),
              e.what());
    return false;
  }
}

// 访问器实现
std::string Config::getServerHost() const {
  const json server = configSection(config_data_, "server");
  return get<std::string>(server, "host", "0.0.0.0");
}

int Config::getServerPort() const {
  const json server = configSection(config_data_, "server");
  return get<int>(server, "port", 8080);
}

int Config::getConnectionTimeoutMs() const {
  const json server = configSection(config_data_, "server");
  return get<int>(server, "connection_timeout_ms", 30000);
}

int Config::getWorkerThreadCount() const {
  const json testing = configSection(config_data_, "testing");
  return get<int>(testing, "worker_thread_count", 4);
}

int Config::getMaxConcurrentTests() const {
  const json testing = configSection(config_data_, "testing");
  return get<int>(testing, "max_concurrent_tests", 4);
}

std::string Config::getTestDataPath() const {
  const json testing = configSection(config_data_, "testing");
  return get<std::string>(testing, "test_data_path", "./testData");
}

int Config::getResultRetentionSeconds() const {
  const json testing = configSection(config_data_, "testing");
  return get<int>(testing, "result_retention_seconds", 600);
}

int Config::getMaxStoredResults() const {
  const json testing = configSection(config_data_, "testing");
  return get<int>(testing, "max_stored_results", 1000);
}

bool Config::getKeepRunnerWorkDir() const {
  const json testing = configSection(config_data_, "testing");
  return get<bool>(testing, "keep_work_dir", false);
}

size_t Config::getBufferSize() const {
  const json performance = configSection(config_data_, "performance");
  return get<size_t>(performance, "buffer_size", 8192);
}

std::string Config::getLogFilePath() const {
  const json logging = configSection(config_data_, "logging");
  return get<std::string>(logging, "file_path", "application.log");
}

std::string Config::getLogLevel() const {
  const json logging = configSection(config_data_, "logging");
  return get<std::string>(logging, "level", "INFO");
}

uint32_t Config::getSjudgeRunUid() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<uint32_t>(sjudge, "run_uid", 0);
}

uint32_t Config::getSjudgeRunGid() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<uint32_t>(sjudge, "run_gid", 0);
}

bool Config::getSjudgeClearSupplementaryGroups() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<bool>(sjudge, "clear_supplementary_groups", false);
}

bool Config::getSjudgeEnableSeccomp() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<bool>(sjudge, "enable_seccomp", false);
}

bool Config::getSjudgeSeccompDenyNetwork() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<bool>(sjudge, "seccomp_deny_network", true);
}

bool Config::getSjudgeSeccompDenyProcessSpawn() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<bool>(sjudge, "seccomp_deny_process_spawn", true);
}

std::string Config::getSjudgeCgroupPath() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<std::string>(sjudge, "cgroup_path", "");
}

uint64_t Config::getSjudgeCgroupMemoryMaxBytes() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<uint64_t>(sjudge, "cgroup_memory_max_bytes", 0);
}

uint32_t Config::getSjudgeCgroupPidsMax() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<uint32_t>(sjudge, "cgroup_pids_max", 0);
}

uint64_t Config::getSjudgeCgroupCpuMaxQuotaUs() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<uint64_t>(sjudge, "cgroup_cpu_max_quota_us", 0);
}

uint64_t Config::getSjudgeCgroupCpuMaxPeriodUs() const {
  const json sjudge = configSection(config_data_, "sjudge");
  return get<uint64_t>(sjudge, "cgroup_cpu_max_period_us", 0);
}
