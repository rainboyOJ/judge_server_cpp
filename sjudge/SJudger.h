#pragma once

#include "JudgeTypes.h"

/**
 * @brief 运行一次受控评测进程。
 *
 * 这是内置 sjudger 的稳定入口。调用方只需要填好 `judge_config`，
 * 不需要直接处理 fork/execve/wait4/setrlimit 等细节。
 *
 * @param config 执行配置，包括程序路径、输入输出路径和资源限制。
 * @return judge_result 底层执行结果；若 result 为 SUCCESS，runner
 * 仍需比较输出。
 */
judge_result run_sjudger(const judge_config &config);
