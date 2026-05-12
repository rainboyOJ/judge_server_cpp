#pragma once

#include "sjudge_call.h"

/**
 * @brief 子进程入口：准备运行环境并执行用户程序。
 *
 * 这个函数只应在 fork 后的子进程中调用。它会在任一准备步骤失败时
 * `_exit(error_code)`，由父进程把这些错误识别为 SYSTEM_ERROR。
 *
 * @param config 子进程执行配置。
 */
[[noreturn]] void run_child_process_or_exit(const judge_config &config);
