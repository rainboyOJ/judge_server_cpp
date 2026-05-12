#pragma once

#include "ParentMonitor.h"
#include "sjudge_call.h"

int map_monitor_result_to_judge_code(const judge_config &config,
                                     const monitor_result &monitor);
