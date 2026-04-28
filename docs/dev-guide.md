# 开发指南

## 当前架构概览

本项目已从"socket 线程里同步跑完整次评测"切到"旧 TCP 外壳 + 新异步判题后端"：

- 前台接入：`TcpServer`、`ConnectionRegistry`、`ClientSockets`、`JudgeProtocol`
- 异步调度：`SubmissionQueue`、`JudgeWorkerPool`、`SubmissionNotifier`
- 判题主链路：`SubmissionService`、`RunnerFactory`、`JudgeCore`、`ResultStore`
- 旧兼容层：`testBox`、`resultContainer`、部分 `ClientSockets` / `test_process` 逻辑

所有源文件在单一 `src/` 目录树下，`#include` 以 `src/` 为根（如 `#include "network/TcpServer.h"`）。

如果改动的是提交协议、异步任务流、结果查询或结果推送，优先沿着 async 主链路思考，而不是回到旧 `testBox` 流程里补逻辑。

## 构建命令

```bash
# 配置 (Debug 构建会定义 MUDEBUG，启用 LOG_DEBUG)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# 启动服务
./build/judge_server config/config.json
```

## 模块关键源文件

各模块关键文件如下，所有路径相对于仓库根：

### 协议相关

- `src/common/SubmissionTypes.h` — 核心数据结构与枚举
- `src/protocol/JudgeProtocol.h`
- `src/protocol/JudgeProtocol.cpp` — JSON 编解码
- `src/network/ClientSockets.cpp` — 协议分发
- `test/test_protocol_codec.cpp`
- `test/test_async_submission_flow.cpp`
- `test/test_integration_tcp_cpp_python.cpp`

注意：
- `submit` 和 `query_result` 都由 `ClientSockets::deal_events()` 直接分流。
- `encodeResult()` 会根据终态/非终态自动在 `submission_update` 与 `submission_finished` 之间切换。
- 错误包仍走兼容格式，不要误以为所有响应都有 `type`。

### 调度与异步流程

- `src/dispatch/SubmissionTask.h`
- `src/dispatch/SubmissionQueue.h`
- `src/dispatch/SubmissionQueue.cpp`
- `src/dispatch/JudgeWorkerPool.h`
- `src/dispatch/JudgeWorkerPool.cpp`
- `src/dispatch/SubmissionNotifier.h`
- `src/network/ClientSockets.cpp` — notifier 实现方
- `main.cpp`
- `test/test_judge_worker_pool.cpp`
- `test/test_async_submission_flow.cpp`

建议按这个脑图理解：
- `ConnectionRegistry` 管理连接槽位 / session / 待发送 frame
- `ClientSockets` 创建 `submission_id`
- 组装 `SubmissionTask{submission_id, request, reply_channel_id}`
- 入 `SubmissionQueue`
- `JudgeWorkerPool` 后台消费
- notifier 把生命周期事件回调给 `ClientSockets`
- `ClientSockets` 再决定是即时发送还是先 defer 到 ack 后发送

### 评测流程

- `src/pipeline/SubmissionService.h`
- `src/pipeline/SubmissionService.cpp`
- `src/pipeline/ResultStore.h`
- `src/pipeline/ResultStore.cpp`
- `src/pipeline/JudgeCore.h`
- `src/pipeline/JudgeCore.cpp`
- `test/test_submission_service.cpp`
- `test/test_result_store.cpp`
- `test/test_judge_core.cpp`

关键约束：
- `createSubmission()` 只建单据，不跑评测。
- `processSubmission()` 才推进状态机并写中间快照。
- `submit()` 只是兼容性的同步包装，不是 async 主流程入口。

### 语言 Runner

- `src/runner/ILanguageRunner.h` — runner 接口
- `src/runner/RunnerFactory.h`
- `src/runner/RunnerFactory.cpp`
- `src/runner/CppRunner.h` / `src/runner/CppRunner.cpp`
- `src/runner/PythonRunner.h` / `src/runner/PythonRunner.cpp`
- `src/test_process/RunnerCompat.h` — 兼容接口
- `test/test_runner_factory.cpp`
- `test/test_cpp_runner.cpp`
- `test/test_python_runner.cpp`

### 网络与连接

- `src/network/TcpServer.h` / `src/network/TcpServer.cpp`
- `src/network/ClientSockets.h` / `src/network/ClientSockets.cpp`
- `src/network/ConnectionRegistry.h` / `src/network/ConnectionRegistry.cpp`
- `src/network/ConnectionSlot.h`
- `src/network/Buffer.h` / `src/network/Buffer.cpp`

### 旧系统（legacy）

- `src/legacy/testBox.h` / `src/legacy/testBox.cpp`
- `src/legacy/testPointBox.h` / `src/legacy/testPointBox.cpp`
- `src/legacy/resultContainer.h` / `src/legacy/resultContainer.cpp`
- `src/legacy/workThreadPool.h` / `src/legacy/workThreadPool.cpp`

## 修改指引

### 改协议

改 `src/protocol/JudgeProtocol.cpp` 和 `src/common/SubmissionTypes.h` 后，同步更新：
- Node 客户端常量 `test/nodejs/lib/constants.js`
- 文档 `docs/protocol.md`
- 协议测试 `test/test_protocol_codec.cpp`

### 改异步调度

改 `SubmissionQueue` 或 `JudgeWorkerPool` 后，必跑 `test_judge_worker_pool` 和 `test_async_submission_flow`。

### 改评测流程

改 `SubmissionService` 或 `ResultStore` 后，必跑 `test_submission_service`、`test_result_store` 和 `test_judge_core`。

### 加新语言

参见 `docs/runner-extension.md`。涉及 `src/runner/`、`src/common/SubmissionTypes.h`、`src/protocol/JudgeProtocol.cpp` 和 `CMakeLists.txt`。

## 代码约定

- 使用 C++17。
- 小步修改，避免顺手大重构。
- 新逻辑优先复用 `SubmissionRequest` / `SubmissionResult` / `SubmissionCaseResult` / `SubmissionTask`。
- 错误信息保持短而可定位，例如 `unsupported language`、`failed to create submission`、`submission queue unavailable`。
- 运行期状态必须遵守 `ResultStore` 的前进式状态机。
- 不要在 socket 线程里重新引入长时间阻塞评测逻辑；评测应继续留在 worker 线程里。

## 调试技巧

- Debug 构建会定义 `MUDEBUG`，可启用 `LOG_DEBUG`。
- TCP framing 问题优先看 `src/network/Buffer.cpp` 的 `read_message()` 和 `ClientSockets` 的 `set_pending_response()`。
- ack/finished 顺序问题优先看 `awaiting_ack_channels_`、`deferred_protocol_messages_`、`mark_channel_ack_sent()`。
- 查询结果异常优先检查 `SubmissionService::query()` 和 `ResultStore::getResult()`。
- 结果异常优先检查 `processSubmission()` 是否提前写成 `FAILED/FINISHED`。

## 当前要诚实面对的坑

- notifier 目前只有 started/finished 两个时机，没有逐 case 或逐状态回调；如果要做实时进度推送，接口本身需要扩展。
- `onSubmissionStarted()` 回调发生在 `processSubmission()` 之前，所以当前 push 路径通常不会主动发出有内容的 `submission_update`。
- `SubmissionQueue` 是内存无界队列，没有容量限制、持久化、重试和优先级调度。
- `ClientSockets` 已经比之前更聚焦业务桥接，但连接槽位、fd 生命周期和旧 `testBox` 仍然有兼容性耦合。
- 当前题目限制没有配置化；如果要做真正的题目模型扩展，不能只在 runner 里硬编码。
