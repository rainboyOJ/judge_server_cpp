
#pragma once

/**
 * @file sjudge_call.h
 * @brief 兼容头文件，统一暴露内置 sjudger 和旧外部 sjudge 调用入口。
 *
 * 新代码优先 include `SJudger.h`。保留本文件是为了让已有 runner 和测试
 * 不需要一次性改 include 路径。
 */

#include "ExternalSjudgeCompat.h"
#include "SJudger.h"
