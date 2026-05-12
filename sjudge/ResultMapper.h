#pragma once

#include "ParentMonitor.h"
#include "sjudge_call.h"

/**
 * @brief 把父进程监控结果映射成稳定的 sjudger verdict。
 *
 * 这里只处理“用户程序执行结果”的语义，如 TLE/MLE/RE/SUCCESS。
 * fork、dup2、setrlimit、execve 等基础设施错误由 `run_sjudger()` 在调用
 * 本函数前先识别为 SYSTEM_ERROR。
 *
 * @param config 原始执行配置，用于读取资源限制。
 * @param monitor 父进程采集到的子进程状态和资源用量。
 * @return judgeResult_id 中的 verdict 类结果码。
 */
int map_monitor_result_to_judge_code(const judge_config &config,
                                     const monitor_result &monitor);
