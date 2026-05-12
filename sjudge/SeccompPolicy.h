#pragma once

#include "sjudge_call.h"

/**
 * @brief 在子进程中加载 seccomp 策略。
 *
 * 当 CMake 找到 libseccomp 时会编译真实实现；否则回退到 stub。
 * 真实实现默认不启用，只有 `judge_config::enable_seccomp` 为 true 时才加载。
 * 当前真实策略采用“默认允许 + 禁止网络/派生进程”的方式，便于兼容 C++/Python
 * 动态运行时；后续可以按语言演进为白名单策略。
 *
 * @param config 当前执行配置，供未来按语言/路径选择策略使用。
 * @param error_code 失败时写入 LOAD_SECCOMP_FAILED 等错误码。
 * @return true 策略加载成功或 stub 放行；false 加载失败。
 */
bool apply_seccomp_if_enabled(const judge_config &config, int &error_code);
