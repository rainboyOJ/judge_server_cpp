#pragma once

#include "JudgeTypes.h"

/**
 * @brief 调用历史外部 `/usr/bin/sjudge` 二进制的兼容入口。
 *
 * 当前 runner 已经走内置 `run_sjudger()`。这个函数保留给旧代码或手工
 * 兼容场景使用；理解内置 sjudger 架构时可以最后阅读。
 *
 * @param sjudge_binary_path 外部 sjudge 可执行文件路径。
 * @param config 旧 sjudge 命令行参数来源。
 * @return judge_result 从外部 sjudge 文本输出解析出的结果。
 */
judge_result call_sjudge(const char *sjudge_binary_path,
                         const judge_config &config);
