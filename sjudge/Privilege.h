#pragma once

#include "JudgeTypes.h"

/**
 * @brief 在子进程中按配置执行 gid/uid 降权。
 *
 * 该函数只应在 fork 后的子进程调用。降权必须发生在需要较高权限的准备步骤
 * 之后、execve 用户程序之前。默认 `run_uid == 0 && run_gid == 0` 时不做任何事。
 *
 * @param config 执行配置。
 * @param error_code 失败时写入 SETUID_FAILED。
 * @return true 降权成功或未启用；false 降权失败。
 */
bool drop_privileges_if_configured(const judge_config &config, int &error_code);
