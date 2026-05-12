#pragma once

#include "JudgeTypes.h"

/**
 * @brief 按配置写入 cgroup v2 限制，并把当前子进程加入 cgroup。
 *
 * 该函数面向 cgroup v2。调用方需要提前创建好 `config.cgroup_path`，
 * 并保证 judge_server 进程有权限写入其中的 `cgroup.procs`、`memory.max`、
 * `pids.max` 和 `cpu.max`。为空路径表示不启用 cgroup。
 *
 * @param config 执行配置。
 * @param error_code 失败时写入 CGROUP_FAILED。
 * @return true cgroup 未启用或加入成功；false 写入失败。
 */
bool apply_cgroup_if_configured(const judge_config &config, int &error_code);
