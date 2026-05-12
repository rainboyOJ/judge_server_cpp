#pragma once

#include "JudgeTypes.h"

judge_result call_sjudge(const char *sjudge_binary_path,
                         const judge_config &config);
