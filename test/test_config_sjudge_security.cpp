#include "common/Config.h"

#include <cassert>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

fs::path write_test_config() {
  const fs::path path =
      fs::temp_directory_path() / "boxTest_sjudge_config.json";
  std::ofstream out(path);
  out << R"({
  "sjudge": {
    "run_uid": 1001,
    "run_gid": 1002,
    "clear_supplementary_groups": true,
    "enable_seccomp": true,
    "seccomp_deny_network": false,
    "seccomp_deny_process_spawn": false,
    "cgroup_path": "/sys/fs/cgroup/oj",
    "cgroup_memory_max_bytes": 67108864,
    "cgroup_pids_max": 32,
    "cgroup_cpu_max_quota_us": 50000,
    "cgroup_cpu_max_period_us": 100000
  }
})";
  return path;
}

void test_sjudge_security_config_accessors() {
  const fs::path config_path = write_test_config();
  Config &config = Config::getInstance();

  assert(config.loadFromFile(config_path.string()));
  assert(config.getSjudgeRunUid() == 1001);
  assert(config.getSjudgeRunGid() == 1002);
  assert(config.getSjudgeClearSupplementaryGroups());
  assert(config.getSjudgeEnableSeccomp());
  assert(!config.getSjudgeSeccompDenyNetwork());
  assert(!config.getSjudgeSeccompDenyProcessSpawn());
  assert(config.getSjudgeCgroupPath() == "/sys/fs/cgroup/oj");
  assert(config.getSjudgeCgroupMemoryMaxBytes() == 67108864);
  assert(config.getSjudgeCgroupPidsMax() == 32);
  assert(config.getSjudgeCgroupCpuMaxQuotaUs() == 50000);
  assert(config.getSjudgeCgroupCpuMaxPeriodUs() == 100000);

  std::error_code ec;
  fs::remove(config_path, ec);
}

} // namespace

int main() {
  test_sjudge_security_config_accessors();
  return 0;
}
