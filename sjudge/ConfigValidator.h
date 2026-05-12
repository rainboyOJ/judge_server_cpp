#pragma once

struct judge_config;

bool validate_judge_config(const judge_config &config, int &error_code);
