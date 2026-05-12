#include "Privilege.h"

#include <grp.h>
#include <sys/types.h>
#include <unistd.h>

bool drop_privileges_if_configured(const judge_config &config,
                                   int &error_code) {
  if (config.run_uid == 0 && config.run_gid == 0 &&
      !config.clear_supplementary_groups) {
    error_code = 0;
    return true;
  }

  if (config.clear_supplementary_groups && setgroups(0, nullptr) != 0) {
    error_code = SETUID_FAILED;
    return false;
  }

  if (config.run_gid != 0 && setgid(static_cast<gid_t>(config.run_gid)) != 0) {
    error_code = SETUID_FAILED;
    return false;
  }

  if (config.run_uid != 0 && setuid(static_cast<uid_t>(config.run_uid)) != 0) {
    error_code = SETUID_FAILED;
    return false;
  }

  error_code = 0;
  return true;
}
