#include "CgroupSupport.h"

#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

bool write_control_file(const fs::path &path, const std::string &value) {
  std::ofstream stream(path);
  if (!stream.good()) {
    return false;
  }
  stream << value;
  return stream.good();
}

bool write_optional_limit(const fs::path &dir, const char *file,
                          uint64_t value) {
  if (value == 0) {
    return true;
  }
  return write_control_file(dir / file, std::to_string(value));
}

} // namespace

bool apply_cgroup_if_configured(const judge_config &config, int &error_code) {
  if (config.cgroup_path.empty()) {
    error_code = 0;
    return true;
  }

  const fs::path cgroup_dir = config.cgroup_path;
  if (!fs::is_directory(cgroup_dir)) {
    error_code = CGROUP_FAILED;
    return false;
  }

  if (!write_optional_limit(cgroup_dir, "memory.max",
                            config.cgroup_memory_max_bytes) ||
      !write_optional_limit(cgroup_dir, "pids.max", config.cgroup_pids_max)) {
    error_code = CGROUP_FAILED;
    return false;
  }

  if (config.cgroup_cpu_max_quota_us > 0) {
    const uint64_t period = config.cgroup_cpu_max_period_us > 0
                                ? config.cgroup_cpu_max_period_us
                                : 100000;
    if (!write_control_file(cgroup_dir / "cpu.max",
                            std::to_string(config.cgroup_cpu_max_quota_us) +
                                " " + std::to_string(period))) {
      error_code = CGROUP_FAILED;
      return false;
    }
  }

  if (!write_control_file(cgroup_dir / "cgroup.procs",
                          std::to_string(static_cast<long long>(getpid())))) {
    error_code = CGROUP_FAILED;
    return false;
  }

  error_code = 0;
  return true;
}
