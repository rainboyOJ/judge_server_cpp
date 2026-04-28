# 架构说明

## 概览

本项目是一个 async OJ 判题后端，所有源码位于单一 `src/` 目录树下。核心思路是"旧 TCP 外壳 + 新异步判题后端"的过渡架构：

- `TcpServer`：基于 `select` + `eventfd` 的事件循环外壳。
- `ClientSockets`：协议接入、submission 入队、查询分流、结果回包桥接。
- `ConnectionRegistry` / `ConnectionSlot`：连接槽位、session、待发送缓冲与 `fd_set` 参与逻辑。
- `SubmissionService`：拆成 `createSubmission()`（建单据）和 `processSubmission()`（跑评测）两段。
- `SubmissionQueue`：线程安全待评测任务队列。
- `JudgeWorkerPool`：后台线程消费队列并调用 `processSubmission()`。
- `SubmissionNotifier`：worker 生命周期事件回传给 socket 层。
- `ResultStore`：线程安全保存提交快照，`query_result` 和最终推送都从这里读取最新状态。

核心入口：`main.cpp` 读取 `config/config.json`，构造 `SubmissionQueue`、`ClientSockets`、`JudgeWorkerPool`，再启动 `TcpServer`。

## 目录结构

| 目录 | 职责 |
|---|---|
| `src/common/` | 共享数据结构（SubmissionTypes.h）、配置（Config）、日志（Logger）、基础类型（Types.h、Result.h） |
| `src/network/` | TCP Server（TcpServer）、客户端连接管理（ClientSockets）、连接注册表（ConnectionRegistry） |
| `src/dispatch/` | 任务队列（SubmissionQueue）、Worker 线程池（JudgeWorkerPool）、通知接口（SubmissionNotifier） |
| `src/runner/` | 语言 Runner 接口与实现（CppRunner、PythonRunner）、工厂（RunnerFactory） |
| `src/pipeline/` | 评测编排（SubmissionService）、判题归并（JudgeCore）、结果存储（ResultStore） |
| `src/protocol/` | JSON 协议编解码（JudgeProtocol） |
| `src/legacy/` | 旧系统兼容层（testBox、workThreadPool、resultContainer、testPointBox 等） |
| `src/test_process/` | 单点测试执行辅助（Compile、TestOneSinglePoint、RunnerCompat） |

## 网络层 (src/network/)

`TcpServer` 基于 `select` + `eventfd` 实现事件循环：

- 监听 server socket，接受新连接。
- 维护 `eventfd` 作为跨线程唤醒源。
- 在 `select(nullptr)` 中阻塞等待事件，把 `fd_set` 交给 `ClientSockets` 处理。

连接状态管理由 `ConnectionRegistry` / `ConnectionSlot` 负责：

- 读：从 input buffer 累积数据。
- 解帧：读取前 4 字节大端长度，再读取完整 JSON body。
- 写：响应同样带 4 字节长度前缀，支持部分写出后的续传。
- 管理逻辑槽位、fd/session 映射、待发送 frame 与 `fd_set` 参与逻辑。

`ClientSockets` 继承 `SubmissionNotifier`，将 worker 事件转换为 socket 回包。同时保留旧 `testBox` 槽位生命周期和 fallback 回写分支，是兼容层而非全新的 reactor 模块。

## 协议层 (src/protocol/)

`JudgeProtocol` 支持两类请求：

- `submit`
- `query_result`

支持的响应类型：

- `submission_ack`：提交创建成功且任务已入队。
- `submission_update`：查询到非终态快照时返回。
- `submission_finished`：查询到终态快照时返回，或 worker 完成后向原连接推送。

## 调度层 (src/dispatch/)

### SubmissionQueue

- 内部是 `std::queue<SubmissionTask>` + `mutex` + `condition_variable`。
- `push()` 在队列关闭后返回 `false`。
- `pop()` 阻塞等待任务；若 shutdown 后队列为空则返回 `false`，worker 退出。

### JudgeWorkerPool

- 启动时按 `worker_count` 创建后台线程。
- 每个 worker 在 `workerLoop()` 中循环消费 `SubmissionTask`。
- 每个任务执行顺序固定为：`onSubmissionStarted(task)` → `service_.processSubmission(...)` → `onSubmissionFinished(task)`。
- `stop()` 先关闭 `SubmissionQueue`，再 `join` 全部线程。

### SubmissionNotifier

- 薄接口，只有 `onSubmissionStarted()` / `onSubmissionFinished()` 两个 hook。
- `ClientSockets` 继承它，把 worker 事件转换成 socket 回包。
- notifier 不直接持有结果；总是回查 `SubmissionService::query()` / `ResultStore` 里的最新快照。

## 管道层 (src/pipeline/)

`SubmissionService` 职责拆成两段：

1. `createSubmission(request)`：向 `ResultStore` 创建初始记录，返回服务端生成的 `submission_id`，初始状态 `QUEUED`。
2. `processSubmission(submission_id, request)`：真正执行评测，依次推进 `PREPARING → COMPILING → RUNNING → FINISHED/FAILED`，每次跑完测试点就把 `case_results` 快照写回 `ResultStore`。

`submit(request)` 仍保留为兼容包装，但异步主线不再走它。

`JudgeCore` 按固定优先级归并 case verdict：

`CE > SYSTEM_ERROR > TLE > RE/MLE/OLE > WA/PE > UNKNOWN > AC > PENDING`

`ResultStore` 用互斥锁保护 `unordered_map<int, SubmissionResult>`，限制状态只能单向推进。

## 执行层 (src/runner/)

- `ILanguageRunner`：统一定义 `prepare()`、`compile()`、`runCase()` 接口。
- `RunnerFactory`：当前支持 C++（lang=0）和 Python（lang=2）。
- `CppRunner` / `PythonRunner`：约定工作目录 `/tmp/oj_compile_<submission_id>`。
- `RunnerCompat`（`src/test_process/`）：兼容接口，用于复用旧执行逻辑。

## 旧系统层 (src/legacy/)

- `testBox`：旧评测槽位管理器，`main.cpp` 仍创建它，`ClientSockets` 仍依赖它分配 `testBoxId`。
- `workThreadPool`：旧线程池。
- `resultContainer`：旧结果容器。
- `testPointBox`：旧单点评测逻辑。
- `memPool`、`static_loop_queue`、`testQueue`：旧内存/队列工具。

当前最准确的理解是：前端网络壳仍旧，后端判题主流程已异步化。

## 请求生命周期

### `submit` 流程

```
client
  → TCP framed JSON (type=submit)
  → ClientSockets::read_message
  → JudgeProtocol::decodeRequest
  → SubmissionService::createSubmission
  → ResultStore (QUEUED)
  → SubmissionQueue::push
  → immediate submission_ack
  → JudgeWorkerPool worker pop task
  → SubmissionNotifier hooks
  → SubmissionService::processSubmission
  → ResultStore snapshots updated
  → ClientSockets pushes submission_finished if reply channel still valid
```

### `query_result` 流程

```
client
  → TCP framed JSON (type=query_result)
  → ClientSockets::read_message
  → JudgeProtocol::decodeQueryRequest
  → SubmissionService::query
  → ResultStore latest snapshot
  → JudgeProtocol::encodeResult
  → submission_update or submission_finished
```

## 推送与查询的关系

当前同时支持"原连接推送最终结果"和"后续主动查询"：

- 提交成功后，原连接先收到 `submission_ack`。
- worker 完成后，如果原连接对应的 `reply_channel_id` 仍有效，`ClientSockets::onSubmissionFinished()` 会把最终快照排进该连接的待发送队列。
- 如果连接已断开，最终快照不会主动补发，但 `ResultStore` 里仍保留结果，后续新连接可以通过 `query_result` 取回。

## ack 屏障机制

`ClientSockets` 实现了 ack 屏障：

- 任务刚入队时先把 channel 标记为 `awaiting_ack`。
- notifier 在 ack 尚未发出前产生的协议消息会先进入 `deferred_protocol_messages_`。
- `mark_channel_ack_sent()` 后再把这些延迟消息回灌到正常发送路径。

这保证了同一连接上不会出现"最终结果先于 ack 发出"的乱序。

## eventfd 唤醒

后台线程把连接切到 `WRITABLE` 或挂上待发送协议响应时，会通过 `eventfd` 显式唤醒阻塞中的 `TcpServer::select()`，因此主循环不再依赖超时轮询发现跨线程状态变化。

## 当前限制

- `SubmissionNotifier` 只有 started/finished 两个 hook；虽然 `ResultStore` 内部有每个 case 的中间快照，但当前不会逐点主动推送 `submission_update`。
- `onSubmissionStarted()` 发生在 `processSubmission()` 之前，当前 `ClientSockets` 会跳过 `QUEUED` 快照，因此大多数实际连接看到的是 `submission_ack` 然后直接 `submission_finished`。
- `query_result` 只能按 `submission_id` 拉取"当前最新快照"，没有订阅、长轮询或批量查询接口。
- 没有任务取消、重试、优先级、背压指标或队列持久化；进程退出后内存队列和结果都会丢失。
- `SubmissionQueue` 是无界内存队列，没有显式容量限制。
- `RunnerFactory` 只支持 C++/Python；`SubmissionLanguage::C` 仍是保留值。
- 测试点时间/内存限制硬编码在 `SubmissionService::load_cases_for_problem()`，尚未从题目配置读取。
- fallback 执行路径不是完整沙箱；没有 `/usr/bin/sjudge` 时只保证基础真实时间超时。
