#pragma once

struct judge_config;

/**
 * @brief 校验评测配置是否足够启动子进程。
 *
 * 当前只做最小规则校验：`exe_path` 必须非空。后续如果要校验工作目录、
 * 文件路径、参数数量或资源限制范围，应继续放在这里，避免散落到总控流程。
 *
 * @param config 待校验配置。
 * @param error_code 校验失败时写入具体错误码。
 * @return true 配置可继续执行；false 配置非法。
 */
bool validate_judge_config(const judge_config &config, int &error_code);
