#pragma once

#include "sjudge_call.h"

bool apply_seccomp_if_enabled(const judge_config &config, int &error_code);
