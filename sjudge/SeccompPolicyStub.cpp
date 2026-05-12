#include "SeccompPolicy.h"

/**
 * @brief libseccomp 不可用或 CMake 关闭 seccomp 时的空实现。
 *
 * stub 明确返回成功，让调用方可以保留同一条 ChildSetup 流程。
 */
bool apply_seccomp_if_enabled(const judge_config &, int &) { return true; }
