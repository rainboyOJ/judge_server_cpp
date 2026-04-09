# 开发指南

## 先理解当前代码形态

这个 worktree 现在最关键的变化，不是 runner 或 verdict 规则，而是提交流程已经拆成“前台接入 + 后台异步处理”两段：

- 前台接入：`TcpServer`、`ClientSockets`、`JudgeProtocol`
- 异步调度：`SubmissionQueue`、`JudgeWorkerPool`、`SubmissionNotifier`
- 判题主链路：`SubmissionService`、`RunnerFactory`、`JudgeCore`、`ResultStore`
- 旧兼容层：`testBox`、`resultContainer`、部分 `client_sockets` / `test_process` 逻辑

如果改动的是提交协议、异步任务流、结果查询或结果推送，优先沿着这条 async 主链路思考，而不是回到旧 `testBox` 流程里补逻辑。

## 推荐开发流程

### 1. 配置构建目录

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

### 2. 先跑定向测试

按改动范围选择：

```bash
ctest --test-dir build -R '^test_protocol_codec$' --output-on-failure
ctest --test-dir build -R '^test_submission_service$' --output-on-failure
ctest --test-dir build -R '^test_judge_worker_pool$' --output-on-failure
ctest --test-dir build -R '^test_async_submission_flow$' --output-on-failure
ctest --test-dir build -R '^test_integration_tcp_cpp_python$' --output-on-failure
ctest --test-dir build -R '^test_(cpp|python)_runner$' --output-on-failure
```

### 3. 再跑全量

```bash
ctest --test-dir build --output-on-failure
```

### 4. 如果改到 Node 协议辅助代码

```bash
cd test/nodejs
npm test
```

## 模块改动建议

### 协议相关

优先看这些文件：

- `include/common/SubmissionTypes.h`
- `include/protocol/JudgeProtocol.h`
- `src/protocol/JudgeProtocol.cpp`
- `src/client_sockets.cpp`
- `test/test_protocol_codec.cpp`
- `test/test_async_submission_flow.cpp`
- `test/test_integration_tcp_cpp_python.cpp`

现在要特别注意：

- `submit` 和 `query_result` 都由 `ClientSockets::deal_events()` 直接分流。
- `encodeResult()` 会根据终态/非终态自动在 `submission_update` 与 `submission_finished` 之间切换。
- 错误包仍走兼容格式，不要误以为所有响应都有 `type`。

### 调度与异步流程相关

优先看这些文件：

- `include/dispatch/SubmissionTask.h`
- `include/dispatch/SubmissionQueue.h`
- `src/dispatch/SubmissionQueue.cpp`
- `include/dispatch/JudgeWorkerPool.h`
- `src/dispatch/JudgeWorkerPool.cpp`
- `include/dispatch/SubmissionNotifier.h`
- `src/client_sockets.cpp`
- `main.cpp`
- `test/test_judge_worker_pool.cpp`
- `test/test_async_submission_flow.cpp`

建议按这个脑图理解：

- `ClientSockets` 创建 `submission_id`
- 组装 `SubmissionTask{submission_id, request, reply_channel_id}`
- 入 `SubmissionQueue`
- `JudgeWorkerPool` 后台消费
- notifier 把生命周期事件回调给 `ClientSockets`
- `ClientSockets` 再决定是即时发送还是先 defer 到 ack 后发送

### 服务流程相关

优先看这些文件：

- `include/service/SubmissionService.h`
- `src/service/SubmissionService.cpp`
- `include/store/ResultStore.h`
- `src/store/ResultStore.cpp`
- `include/judge/JudgeCore.h`
- `src/judge/JudgeCore.cpp`
- `test/test_submission_service.cpp`
- `test/test_result_store.cpp`
- `test/test_judge_core.cpp`

现在服务层最重要的约束是：

- `createSubmission()` 只建单据，不跑评测。
- `processSubmission()` 才推进状态机并写中间快照。
- `submit()` 只是兼容性的同步包装，不是 async 主流程入口。

### 新语言相关

改这些文件：

- `include/runner/*.h`
- `src/runner/*.cpp`
- `src/test_process/RunnerCompat.h`（仅在需要扩展复用接口时）
- `src/test_process/TestOneSinglePoint.cpp`
- `test/test_runner_factory.cpp`
- 新的 runner 单测

## 代码约定

- 维持 C++17。
- 小步修改，避免顺手大重构。
- 新逻辑优先复用 `SubmissionRequest` / `SubmissionResult` / `SubmissionCaseResult` / `SubmissionTask`。
- 错误信息保持短而可定位，例如 `unsupported language`、`failed to create submission`、`submission queue unavailable`。
- 运行期状态必须遵守 `ResultStore` 的前进式状态机。
- 不要在 socket 线程里重新引入长时间阻塞评测逻辑；评测应继续留在 worker 线程里。

## 调试技巧

- Debug 构建会定义 `MUDEBUG`，可启用 `LOG_DEBUG`。
- TCP framing 问题优先看 `FdInfo::read_message()` 和 `set_pending_response()`。
- ack/finished 顺序问题优先看 `awaiting_ack_channels_`、`deferred_protocol_messages_`、`mark_channel_ack_sent()`。
- 查询结果异常优先检查 `SubmissionService::query()` 和 `ResultStore::getResult()`。
- 结果异常优先检查 `processSubmission()` 是否提前写成 `FAILED/FINISHED`。

## 当前要诚实面对的坑

- notifier 目前只有 started/finished 两个时机，没有逐 case 或逐状态回调；如果要做实时进度推送，接口本身需要扩展。
- `onSubmissionStarted()` 回调发生在 `processSubmission()` 之前，所以当前 push 路径通常不会主动发出有内容的 `submission_update`。
- `SubmissionQueue` 是内存无界队列，没有容量限制、持久化、重试和优先级调度。
- `ClientSockets` 虽然已经能把最终结果异步推回原连接，但连接槽位、fd 生命周期和旧 `testBox` 仍然深度耦合。
- 当前题目限制没有配置化；如果要做真正的题目模型扩展，不能只在 runner 里硬编码。
