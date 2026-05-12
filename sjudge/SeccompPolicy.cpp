#include "SeccompPolicy.h"

#include <seccomp.h>

namespace {

bool add_rule(scmp_filter_ctx ctx, int action, int syscall_id) {
  return seccomp_rule_add(ctx, action, syscall_id, 0) == 0;
}

bool add_kill_rule_if_available(scmp_filter_ctx ctx, int syscall_id) {
  return add_rule(ctx, SCMP_ACT_KILL_PROCESS, syscall_id);
}

bool deny_network_syscalls(scmp_filter_ctx ctx) {
  return add_kill_rule_if_available(ctx, SCMP_SYS(socket)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(connect)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(accept)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(accept4)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(bind)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(listen)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(sendto)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(recvfrom)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(sendmsg)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(recvmsg)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(shutdown));
}

bool deny_process_spawn_syscalls(scmp_filter_ctx ctx) {
  return add_kill_rule_if_available(ctx, SCMP_SYS(clone)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(fork)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(vfork)) &&
         add_kill_rule_if_available(ctx, SCMP_SYS(clone3));
}

} // namespace

bool apply_seccomp_if_enabled(const judge_config &config, int &error_code) {
  if (!config.enable_seccomp) {
    error_code = 0;
    return true;
  }

  // 当前策略采用“默认允许 + 精准禁止”的保守落地方式。这样能先稳定支持
  // C++/Python 动态运行时，同时补上小型 OJ 最重要的禁网络、禁派生进程边界。
  // 后续如果要切到白名单策略，应按语言分别用 strace 收集 syscall 后再收紧。
  scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
  if (ctx == nullptr) {
    error_code = LOAD_SECCOMP_FAILED;
    return false;
  }

  bool ok = true;
  if (config.seccomp_deny_network) {
    ok = ok && deny_network_syscalls(ctx);
  }
  if (config.seccomp_deny_process_spawn) {
    ok = ok && deny_process_spawn_syscalls(ctx);
  }

  if (!ok || seccomp_load(ctx) != 0) {
    seccomp_release(ctx);
    error_code = LOAD_SECCOMP_FAILED;
    return false;
  }

  seccomp_release(ctx);
  error_code = 0;
  return true;
}
