# judge-server-mvp

一个面向 Linux 的最小在线判题服务端原型，当前重点是把现有 TCP 接入层、统一提交模型、语言 runner、判题汇总和测试体系串起来。

## 当前能力

- TCP 服务端，协议为 `4` 字节大端长度前缀 + JSON 消息体。
- 请求 JSON 兼容旧客户端字段：`uuid`、`pid`、`lang`、`code`。
- 当前实际支持语言：`C++(0)`、`Python(2)`；`C(1)` 仍保留枚举值，但 `RunnerFactory` 还不会创建 runner。
- 结果以统一 JSON 返回，包含 `submission_id`、`status`、`verdict`、`message/msg`、`case_results`。
- 单元测试和集成测试覆盖协议、状态机、runner、服务层和 TCP 端到端流程。

## 快速开始

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/judge_server config/config.json
```

默认监听 `8000` 端口，测试数据默认来自 `testData/`。

## 文档

- `docs/architecture.md`：当前分层架构与请求生命周期
- `docs/protocol.md`：TCP framing、JSON 结构、状态与判题结果
- `docs/runner-extension.md`：如何新增一种语言 runner
- `docs/testing.md`：CTest、单测、Node 脚本与端到端验证
- `docs/deployment-linux.md`：Linux 构建、运行、checker/sjudge 部署
- `docs/dev-guide.md`：开发者工作流与改动建议

## 诚实说明

- socket 层通过 `detach` 线程把请求从 `select` 事件循环移开，但线程内部的 `SubmissionService::submit()` 仍是同步执行，当前不是队列化异步判题系统。
- 如果机器上没有 `/usr/bin/sjudge`，会走 fallback 执行路径：只强制真实时间限制，不做真正的 CPU/内存隔离；`cpu_time_ms` 直接复用 `real_time_ms`。
- 输出比较优先调用 `/judge/checker/fcmp2`；如果没有部署 checker，会退化为内置的按行比较（忽略行尾空白和末尾空行）。
- `test/nodejs/` 下既有新的 JSON 协议库，也保留了旧脚本和旧命名；`package.json` 的 npm scripts 目前仍指向旧入口，需要按文档手动选择脚本。
