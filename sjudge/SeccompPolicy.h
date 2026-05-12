#pragma once

#include "sjudge_call.h"

/**
 * @brief 在子进程中加载 seccomp 策略。
 *
 * 当前仓库仍使用 `SeccompPolicyStub.cpp`，不会真正限制系统调用。真实实现
 * 应在子进程完成 chdir、IO 重定向和 setrlimit 后、execve 前调用。
 *
 * @param config 当前执行配置，供未来按语言/路径选择策略使用。
 * @param error_code 失败时写入 LOAD_SECCOMP_FAILED 等错误码。
 * @return true 策略加载成功或 stub 放行；false 加载失败。
 */
bool apply_seccomp_if_enabled(const judge_config &config, int &error_code);
