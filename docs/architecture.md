# 架构说明

## 概览

当前 worktree 已经从“socket 线程里同步跑完整次评测”切到“旧 TCP 外壳 + 新异步判题后端”的过渡架构：

- `TcpServer` 负责 `select` / `accept` / `eventfd` 唤醒循环本身。
- `ClientSockets` 负责协议接入、submission 入队、查询分流与结果回包桥接。
- `ConnectionRegistry` / `ConnectionSlot` 负责连接槽位、session、待发送缓冲和 `fd_set` 参与逻辑。
- `SubmissionService` 不再只有单体 `submit()` 路径，而是拆成 `createSubmission()` 和 `processSubmission()` 两段。
- `SubmissionQueue` 负责缓存待评测任务。
- `JudgeWorkerPool` 在后台线程里消费队列并调用 `processSubmission()`。
- `SubmissionNotifier` 负责把 worker 生命周期事件回传给 socket 层。
- `ResultStore` 保存提交快照，`query_result` 和最终推送都从这里读取最新状态。

核心入口与层次：

- `main.cpp`：读取 `config/config.json`，构造 `SubmissionQueue`、`ClientSockets`、`JudgeWorkerPool`，再启动 `TcpServer`。
- `src/client_sockets.cpp`：收包、协议解码、创建 submission、入队、立即回 `submission_ack`、处理 `query_result`、接收 notifier 回调并排队回包。
- `src/net/ConnectionRegistry.cpp`：管理逻辑槽位、fd/session 映射、待发送 frame 与 `fd_set` 参与逻辑。
- `src/dispatch/SubmissionQueue.cpp`：线程安全提交队列。
- `src/dispatch/JudgeWorkerPool.cpp`：后台 worker 循环，`pop -> notifier(start) -> processSubmission -> notifier(finish)`。
- `src/service/SubmissionService.cpp`：真正的评测编排。
- `src/protocol/JudgeProtocol.cpp`：JSON 编解码。
- `src/store/ResultStore.cpp`：线程安全保存提交快照，并限制状态只能前进。

## 分层

### 1. 网络接入层

`TcpServer` 仍然基于 `select`，但它现在只负责“事件循环外壳”本身：

- 监听 server socket
- 维护 `eventfd` 唤醒源
- 在阻塞 `select(nullptr)` 中等待真实事件
- 把 `fd_set` 交给上层连接管理器和业务层

连接具体状态已经从 `ClientSockets` 中抽离到 `ConnectionRegistry/ConnectionSlot`：

- 读：从 `FdInfo::input_buffer_` 累积数据。
- 解帧：读取前 `4` 字节大端长度，再读取完整 JSON body。
- 写：响应同样带 `4` 字节长度前缀，并支持部分写出后的续传。

这一层仍保留旧 `testBox` 槽位生命周期和 fallback 回写分支，所以它是兼容层，不是全新的 reactor/network 模块。

### 2. 协议层

`JudgeProtocol` 当前实际支持两类请求：

- `submit`
- `query_result`

当前实际会发出的协议消息：

- `submission_ack`：提交创建成功且任务已入队。
- `submission_update`：查询到非终态快照时返回。
- `submission_finished`：查询到终态快照时返回，或 worker 完成后向原连接推送。

错误包仍通过 `encodeError()` 生成兼容格式，不带 `type` 字段。

### 3. 调度层

#### `SubmissionQueue`

- 内部是 `std::queue<SubmissionTask>` + `mutex` + `condition_variable`。
- `push()` 在队列关闭后返回 `false`。
- `pop()` 阻塞等待任务；若 shutdown 后队列为空则返回 `false`，worker 退出。

#### `JudgeWorkerPool`

- 启动时按 `worker_count` 创建后台线程。
- 每个 worker 在 `workerLoop()` 中循环消费 `SubmissionTask`。
- 每个任务执行顺序固定为：`onSubmissionStarted(task)` -> `service_.processSubmission(...)` -> `onSubmissionFinished(task)`。
- `stop()` 会先关闭 `SubmissionQueue`，再 `join` 全部线程。

#### `SubmissionNotifier`

- 这是一个非常薄的接口，只有 `onSubmissionStarted()` / `onSubmissionFinished()` 两个 hook。
- `ClientSockets` 继承它，用来把 worker 事件转换成 socket 回包。
- notifier 不直接持有结果；它总是回查 `SubmissionService::query()` / `ResultStore` 里的最新快照。

### 4. 服务编排层

`SubmissionService` 的职责已经拆成两段：

1. `createSubmission(request)`
   - 只负责向 `ResultStore` 创建一条初始记录。
   - 返回服务端生成的 `submission_id`。
   - 初始状态固定为 `QUEUED`。

2. `processSubmission(submission_id, request)`
   - 真正执行评测流程。
   - 依次推进 `PREPARING -> COMPILING -> RUNNING -> FINISHED/FAILED`。
   - 每跑完一个测试点就把新的 `case_results` 快照写回 `ResultStore`。
   - 编译失败写成 `FINISHED + CE`；系统级异常写成 `FAILED + SYSTEM_ERROR`。

兼容接口 `submit(request)` 仍保留，但它只是顺序调用 `createSubmission()` 和 `processSubmission()`；异步主线已经不再走它。

### 5. 执行、判题、存储层

- `RunnerFactory`：当前只支持 `C++` 和 `Python`。
- `ILanguageRunner`：统一定义 `prepare()`、`compile()`、`runCase()`。
- `JudgeCore`：按固定优先级归并 case verdict。
- `ResultStore`：用互斥锁保护 `unordered_map<int, SubmissionResult>`，并限制状态只能单向推进。

## 请求生命周期

### `submit`

```text
client
  -> TCP framed JSON(type=submit)
  -> ClientSockets::read_message
  -> JudgeProtocol::decodeRequest
  -> SubmissionService::createSubmission
  -> ResultStore(QUEUED)
  -> SubmissionQueue::push
  -> immediate submission_ack
  -> JudgeWorkerPool worker pop task
  -> SubmissionNotifier hooks
  -> SubmissionService::processSubmission
  -> ResultStore snapshots updated
  -> ClientSockets pushes submission_finished if reply channel still valid
```

### `query_result`

```text
client
  -> TCP framed JSON(type=query_result)
  -> ClientSockets::read_message
  -> JudgeProtocol::decodeQueryRequest
  -> SubmissionService::query
  -> ResultStore latest snapshot
  -> JudgeProtocol::encodeResult
  -> submission_update or submission_finished
```

## 推送与查询的关系

当前实现同时支持“原连接推送最终结果”和“后续主动查询”：

- 提交成功后，原连接先收到 `submission_ack`。
- worker 完成后，如果原连接对应的 `reply_channel_id` 仍有效，`ClientSockets::onSubmissionFinished()` 会把最终快照排进该连接的待发送队列。
- 如果连接已断开，最终快照不会主动补发，但 `ResultStore` 里仍保留结果，后续新连接可以通过 `query_result` 取回。

`ClientSockets` 还实现了一个 ack 屏障：

- 任务刚入队时先把 channel 标记为 `awaiting_ack`。
- notifier 在 ack 尚未发出前产生的协议消息会先进入 `deferred_protocol_messages_`。
- `mark_channel_ack_sent()` 后再把这些延迟消息回灌到正常发送路径。

此外，后台线程把连接切到 `WRITABLE` 或挂上待发送协议响应时，会通过 `eventfd` 显式唤醒阻塞中的 `TcpServer::select()`，因此主循环不再依赖超时轮询发现跨线程状态变化。

这保证了同一连接上不会出现“最终结果先于 ack 发出”的乱序。

## 与旧系统的边界

当前代码仍然不是完全替换旧 `testBox`：

- `main.cpp` 仍创建 `testBox`，`ClientSockets` 仍依赖它分配 `testBoxId`。
- `FdInfo` / `ClientSockets` 的连接管理仍完全沿用旧 socket 结构。
- 写回路径保留 `test_box_->getResult()` fallback，方便尚未迁移的旧链路继续工作。

所以当前最准确的理解是：前端网络壳仍旧，后端判题主流程已异步化。

## 当前限制

- `SubmissionNotifier` 只有 started/finished 两个 hook；虽然 `ResultStore` 内部有每个 case 的中间快照，但当前不会逐点主动推送 `submission_update`。
- `onSubmissionStarted()` 发生在 `processSubmission()` 之前，当前 `ClientSockets` 会跳过 `QUEUED` 快照，因此大多数实际连接看到的是 `submission_ack` 然后直接 `submission_finished`。
- `query_result` 只能按 `submission_id` 拉取“当前最新快照”，没有订阅、长轮询或批量查询接口。
- 没有任务取消、重试、优先级、背压指标或队列持久化；进程退出后内存队列和结果都会丢失。
- `SubmissionQueue` 是无界内存队列，当前没有显式容量限制。
- `RunnerFactory` 只支持 `C++/Python`；`SubmissionLanguage::C` 仍是保留值。
- 测试点时间/内存限制仍硬编码在 `SubmissionService::load_cases_for_problem()`，尚未从题目配置读取。
- fallback 执行路径仍不是完整沙箱；没有 `/usr/bin/sjudge` 时只保证基础真实时间超时。
