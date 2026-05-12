#include "SeccompPolicy.h"

bool apply_seccomp_if_enabled(const judge_config &, int &) {
    return true;
}
