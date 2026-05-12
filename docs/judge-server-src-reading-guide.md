# judge_server 源码阅读指南

这篇文档面向想阅读 `src/` 目录代码的人。目标是让你看完后能理解：

1. `judge_server` 启动后创建了哪些核心对象。
2. 一个 `submit` 请求如何从 TCP 连接进入系统。
3. 任务如何进入队列、被 worker 消费、执行 runner。
4. 结果如何保存、查询、回推给客户端。
5. 每个 `src/` 子目录分别负责什么。

阅读顺序建议：

```text
src/main.cpp
src/common/SubmissionTypes.h
src/network/TcpServer.*
src/network/ClientSockets.*
src/protocol/JudgeProtocol.*
src/dispatch/SubmissionQueue.*
src/dispatch/JudgeWorkerPool.*
src/pipeline/SubmissionService.*
src/runner/*
src/pipeline/ResultStore.*
src/pipeline/JudgeCore.*
```

## 1. 总体理解

`judge_server` 是一个异步判题后端。它不是“收到请求后在网络线程里直接判题”，而是拆成两条线：

1. 网络线程
   - 负责 TCP 连接。
   - 解析请求。
   - 创建 submission。
   - 把任务放入队列。
   - 发送 ack、查询结果、回推最终结果。
2. worker 线程
   - 从队列取任务。
   - 调用 `SubmissionService::processSubmission()`。
   - 编译、运行、汇总结果。
   - 把结果写入 `ResultStore`。

核心数据流：

```text
client
  |
  | 4-byte length + JSON
  v
TcpServer
  |
  v
ClientSockets
  |
  | decode submit/query_result
  v
JudgeProtocol
  |
  +-- submit
  |     |
  |     v
  |   SubmissionService::createSubmission
  |     |
  |     v
  |   ResultStore: QUEUED
  |     |
  |     v
  |   SubmissionQueue::push
  |     |
  |     v
  |   immediate submission_ack
  |     |
  |     v
  |   JudgeWorkerPool worker
  |     |
  |     v
  |   SubmissionService::processSubmission
  |     |
  |     v
  |   RunnerFactory -> CppRunner/PythonRunner
  |     |
  |     v
  |   run_executable_case -> run_sjudger
  |     |
  |     v
  |   ResultStore: FINISHED
  |     |
  |     v
  |   ClientSockets::onSubmissionFinished
  |     |
  |     v
  |   submission_finished
  |
  +-- query_result
        |
        v
      SubmissionService::query
        |
        v
      ResultStore latest snapshot
        |
        v
      submission_update / submission_finished
```

## 2. 程序入口：main.cpp

入口文件是：

```text
src/main.cpp
```

它做的事情很少，但非常关键：

1. 读取配置文件。
2. 创建全局核心对象。
3. 把网络层、队列、worker、service 接起来。
4. 启动 TCP 事件循环。

核心对象创建顺序：

```cpp
SubmissionQueue submission_queue;
ResultStore result_store;
RunnerFactory runner_factory;
JudgeCore judge_core;
SubmissionService submission_service(result_store, runner_factory, judge_core);
ClientSockets client_sockets(...);
JudgeWorkerPool judge_worker_pool(...);
TcpServer server(...);
server->start();
```

理解 `main.cpp` 的关键是：它是“组装器”，不做业务细节。

真正的职责分布是：

| 对象 | 职责 |
| --- | --- |
| `TcpServer` | 监听端口，跑 `select` 事件循环 |
| `ClientSockets` | 管理客户端连接、协议分发、异步回包 |
| `SubmissionQueue` | 网络线程和 worker 线程之间的任务队列 |
| `JudgeWorkerPool` | 后台 worker 线程池 |
| `SubmissionService` | 评测流程编排 |
| `RunnerFactory` | 根据语言创建 runner |
| `JudgeCore` | 汇总多个测试点 verdict |
| `ResultStore` | 保存 submission 最新快照 |

## 3. 共享数据模型：common/

核心文件：

```text
src/common/SubmissionTypes.h
```

这里定义了整个异步评测链路共享的数据结构。

### SubmissionRequest

协议层解码后得到：

```cpp
struct SubmissionRequest {
    int uuid;
    std::string pid;
    SubmissionLanguage language;
    std::string code;
};
```

含义：

- `uuid`：客户端提交的 id。进入执行阶段后会被服务端 submission_id 替换，用作工作目录键。
- `pid`：题目 id，用来从 `testData/<pid>/data` 找测试点。
- `language`：语言类型，目前主要支持 C++ 和 Python。
- `code`：用户代码。

### SubmissionResult

系统内部保存和返回给客户端的结果快照：

```cpp
struct SubmissionResult {
    int submission_id;
    SubmissionStatus status;
    SubmissionVerdict verdict;
    std::string message;
    std::vector<SubmissionCaseResult> case_results;
};
```

它是 `ResultStore` 的核心保存对象。

### SubmissionStatus

状态机：

```text
QUEUED -> PREPARING -> COMPILING -> RUNNING -> FINISHED
                                 \-> FAILED
```

`ResultStore` 会约束状态只能向前推进，防止结果回退。

## 4. 网络外壳：network/TcpServer

核心文件：

```text
src/network/TcpServer.h
src/network/TcpServer.cpp
```

`TcpServer` 是一个很薄的网络事件循环外壳。

它负责：

1. 创建 server socket。
2. `bind()` 到配置端口。
3. `listen()`。
4. 使用 `select()` 等待事件。
5. 接受新连接。
6. 把客户端 fd_set 处理交给 `ClientSockets`。

`TcpServer` 不懂 JSON、不懂 submission、不懂判题。

它通过三个回调和外部协作：

| 回调 | 谁提供 | 作用 |
| --- | --- | --- |
| `on_accept` | `main.cpp` | 新连接到来时交给 `ClientSockets::add_socket()` |
| `on_client_event` | `main.cpp` | select 返回后交给 `ClientSockets::deal_events()` |
| `renew_socket_sets` | `main.cpp` | 每轮 select 前让 `ClientSockets` 填充 read/write fd_set |

### 为什么有 eventfd

后台 worker 线程完成评测后，可能要把结果挂到某个连接的待发送队列。

但主线程可能正阻塞在：

```cpp
select(max_fd + 1, &read_fds, &write_fds, nullptr, nullptr);
```

如果不唤醒它，它不知道某个 socket 已经变成可写。

所以 `TcpServer` 用 `eventfd` 做跨线程唤醒：

```text
worker thread
  -> ClientSockets queues response
  -> wake_callback
  -> TcpServer::wake()
  -> write(eventfd)
  -> select wakes up
```

## 5. 连接和协议接入：network/ClientSockets

核心文件：

```text
src/network/ClientSockets.h
src/network/ClientSockets.cpp
```

`ClientSockets` 是网络层和业务层的桥。

它负责：

1. 管理连接槽位。
2. 读取 TCP frame。
3. 调用 `JudgeProtocol` 解 JSON。
4. 区分 `submit` 和 `query_result`。
5. 对 submit 创建任务并入队。
6. 对 query 查询 `ResultStore` 快照。
7. 接收 worker finished 通知并回推结果。

### ConnectionRegistry / ConnectionSlot

连接状态不直接散落在 `ClientSockets` 里，而是放在：

```text
src/network/ConnectionRegistry.*
src/network/ConnectionSlot.*
```

`ConnectionSlot` 管一个客户端连接：

- socket fd
- session id
- 输入 buffer
- 待发送 response
- 当前 select 状态：READABLE / WRITABLE / IDLE

session id 很重要。它用来防止 fd 复用导致旧 submission 的结果发给新连接。

### TCP frame 格式

协议不是裸 JSON，而是：

```text
4-byte big-endian length + JSON body
```

读取逻辑在 `ConnectionSlot::read_message()`：

1. 先读入 `Buffer`。
2. 如果不足 4 字节，继续等。
3. 读取 4 字节长度。
4. 如果 JSON body 不完整，继续等。
5. 完整后取出 body 交给协议层。

发送时同样会在 `ConnectionSlot::queue_response_locked()` 加 4 字节长度前缀。

## 6. 协议层：protocol/JudgeProtocol

核心文件：

```text
src/protocol/JudgeProtocol.h
src/protocol/JudgeProtocol.cpp
```

`JudgeProtocol` 只做 JSON 编解码，不做业务决策。

支持的请求：

```json
{"type":"submit","uuid":1,"pid":"1000","lang":0,"code":"..."}
```

```json
{"type":"query_result","submission_id":1}
```

支持的响应：

| 响应类型 | 触发场景 |
| --- | --- |
| `submission_ack` | submit 创建成功并入队 |
| `submission_update` | query 查到非终态结果 |
| `submission_finished` | query 查到终态结果，或 worker 完成后主动回推 |
| error JSON | 请求无法解析 |

协议层的一个重要职责是把内部 enum 编成字符串，例如：

```text
SubmissionStatus::RUNNING -> "RUNNING"
SubmissionVerdict::TLE -> "TLE"
```

## 7. 提交处理：SubmissionRequestHandler / QueryRequestHandler

`ClientSockets` 自己不直接写全部 submit/query 逻辑，而是拆给两个 handler：

```text
src/network/SubmissionRequestHandler.*
src/network/QueryRequestHandler.*
```

### submit handler

submit 的核心动作：

1. `SubmissionService::createSubmission(request)`
2. `ResultStore` 创建 `QUEUED` 记录
3. 构造 `SubmissionTask`
4. `SubmissionQueue::push(task)`
5. 返回 `submission_ack`

如果入队失败，会返回错误响应。

### query handler

query 的核心动作：

1. `SubmissionService::query(submission_id)`
2. 从 `ResultStore` 取最新快照
3. 用 `JudgeProtocol::encodeResult()` 编码
4. 非终态返回 `submission_update`
5. 终态返回 `submission_finished`

## 8. ack 屏障和异步回推

相关文件：

```text
src/network/AckBarrier.*
src/network/ReplyChannel.*
src/network/SubmissionEventResponder.*
```

### 为什么需要 ReplyChannel

worker 完成时只知道一个任务完成了，但要把结果发回原连接，需要知道：

```text
slot_id + session_id
```

这两个值组合成 `ReplyChannel`。

session id 用来防止这种错误：

```text
旧连接 A 使用 fd=10
A 提交任务后断开
新连接 B 复用 fd=10
旧任务完成
如果只按 fd/slot 回推，就可能发给 B
```

所以回推前会检查 session 是否仍匹配。

### 为什么需要 AckBarrier

submit 成功时，客户端应该先收到：

```text
submission_ack
```

然后才可能收到：

```text
submission_finished
```

但 worker 线程可能非常快，甚至在 ack 还没真正发送前就完成。

`AckBarrier` 的作用是保证同一 reply channel 上不会出现 finished 先于 ack 的乱序。

## 9. 调度层：dispatch/

核心文件：

```text
src/dispatch/SubmissionQueue.*
src/dispatch/JudgeWorkerPool.*
src/dispatch/SubmissionTask.h
src/dispatch/SubmissionNotifier.h
```

### SubmissionQueue

这是网络线程和 worker 线程之间的阻塞队列。

网络线程：

```cpp
queue.push(task);
```

worker 线程：

```cpp
while (queue.pop(task)) {
    ...
}
```

`shutdown()` 后：

- 不再接受新任务。
- 唤醒所有阻塞 worker。
- 队列空后 `pop()` 返回 false，worker 退出。

### JudgeWorkerPool

构造时创建固定数量 worker 线程。

每个 worker 的循环：

```text
queue.pop(task)
  -> notifier.onSubmissionStarted(task)
  -> service.processSubmission(task.submission_id, task.request)
  -> notifier.onSubmissionFinished(task)
```

`ClientSockets` 实现了 `SubmissionNotifier`，所以 worker 完成后能把最终结果回推到原连接。

## 10. 评测编排：pipeline/SubmissionService

核心文件：

```text
src/pipeline/SubmissionService.h
src/pipeline/SubmissionService.cpp
```

这是业务主流程。

它负责把一份 submission 推过完整状态机：

```text
QUEUED
  -> PREPARING
  -> COMPILING
  -> RUNNING
  -> FINISHED / FAILED
```

### createSubmission

```cpp
int createSubmission(const SubmissionRequest &request);
```

作用：

1. 让 `ResultStore` 创建一条记录。
2. 返回服务端生成的 `submission_id`。

### processSubmission

```cpp
void processSubmission(int submission_id, const SubmissionRequest &request);
```

核心步骤：

1. 写入 `PREPARING` 快照。
2. 通过 `RunnerFactory` 创建语言 runner。
3. 用服务端 `submission_id` 替换 request.uuid，避免工作目录冲突。
4. 调用 `runner->prepare()`。
5. 写入 `COMPILING` 快照。
6. 调用 `runner->compile()`。
7. 写入 `RUNNING` 快照。
8. 从 `testData/<pid>/data` 扫描测试点。
9. 逐个调用 `runner->runCase()`。
10. 每跑完一个 case，就把当前 `case_results` 写回 `ResultStore`。
11. 调用 `JudgeCore::summarize()` 汇总最终 verdict。
12. 写入 `FINISHED` 快照。

如果中途系统错误，会写入 `FAILED` 或带错误 verdict 的终态。

## 11. 结果存储：pipeline/ResultStore

核心文件：

```text
src/pipeline/ResultStore.h
src/pipeline/ResultStore.cpp
```

`ResultStore` 是线程安全的内存结果表。

它负责：

1. 生成自增 `submission_id`。
2. 保存每个 submission 的最新 `SubmissionResult`。
3. 限制状态机只能向前推进。
4. 支持 query 获取最新快照。
5. 对终态结果做惰性清理。

状态机合法跳转在：

```cpp
ResultStore::isValidTransition()
```

如果状态跳转非法，`updateResult()` 会失败。

## 12. verdict 汇总：pipeline/JudgeCore

核心文件：

```text
src/pipeline/JudgeCore.h
src/pipeline/JudgeCore.cpp
```

`JudgeCore` 不执行程序，只汇总多个测试点结果。

当前优先级大致是：

```text
CE
> SYSTEM_ERROR
> TLE
> RE / MLE / OLE
> WA / PE
> UNKNOWN
> AC
> PENDING
```

也就是说，只要有一个高优先级错误，最终 verdict 就会采用它。

## 13. 执行层：runner/

核心文件：

```text
src/runner/ILanguageRunner.h
src/runner/RunnerFactory.*
src/runner/CppRunner.*
src/runner/PythonRunner.*
src/runner/RunnerSupport.h
src/runner/RunnerCompileSupport.cpp
src/runner/RunnerExecutionSupport.cpp
```

### ILanguageRunner

所有语言 runner 都实现三个阶段：

```cpp
prepare()
compile()
runCase()
```

这对应 `SubmissionService` 的状态机：

```text
PREPARING -> COMPILING -> RUNNING
```

### CppRunner

C++ runner 的流程：

1. `prepare()`：创建 `/tmp/oj_compile_<submission_id>`，写入 `solution.cpp`。
2. `compile()`：调用 g++ 编译成 `solution`。
3. `runCase()`：调用 `run_executable_case()` 执行编译产物。

### PythonRunner

Python runner 的流程：

1. `prepare()`：创建工作目录，写入 `solution.py`。
2. `compile()`：用 `python3 -m py_compile` 做语法检查。
3. `runCase()`：生成 `run_python.sh` wrapper，再调用 `run_executable_case()`。

Python 没有真正的编译产物，但仍然保留 compile 阶段，这样服务层状态机对所有语言一致。

### RunnerExecutionSupport

`run_executable_case()` 是 runner 执行单个测试点的共同入口。

它做：

1. 检查可执行文件、输入文件、标准输出文件是否存在。
2. 构造 `judge_config`。
3. 调用内置 `run_sjudger(config)`。
4. 把 sjudger 结果映射成 `SubmissionVerdict`。
5. 如果执行成功，再比较用户输出和标准输出。

注意：

```text
sjudger SUCCESS != 最终 AC
```

`SUCCESS` 只表示程序正常执行结束。输出还要和标准答案比较，可能得到 WA。

## 14. sjudge/ 和 src/ 的关系

虽然 `sjudge/` 不在 `src/` 目录下，但 runner 执行阶段会调用它。

关系是：

```text
SubmissionService
  -> CppRunner/PythonRunner
  -> run_executable_case
  -> run_sjudger
  -> fork/execve/wait4/setrlimit
```

所以理解 `src/runner/RunnerExecutionSupport.cpp` 时，最好结合阅读：

```text
sjudge/readme.md
sjudge/docs/tutorial.md
```

## 15. submit 请求完整时序

下面是最重要的一条链路。

```text
client sends submit frame
  |
  v
TcpServer::select returns readable fd
  |
  v
ClientSockets::deal_events
  |
  v
ConnectionSlot::read_message
  |
  v
ClientSockets::handle_complete_message
  |
  v
JudgeProtocol::decodeRequest
  |
  v
SubmissionRequestHandler::handleSubmit
  |
  v
SubmissionService::createSubmission
  |
  v
ResultStore::createSubmission -> QUEUED
  |
  v
SubmissionQueue::push
  |
  v
ClientSockets queues submission_ack
  |
  v
TcpServer sends ack
  |
  v
JudgeWorkerPool::workerLoop pops task
  |
  v
SubmissionService::processSubmission
  |
  v
Runner prepare/compile/runCase
  |
  v
ResultStore updates snapshots
  |
  v
JudgeWorkerPool calls onSubmissionFinished
  |
  v
ClientSockets queues submission_finished
  |
  v
TcpServer wakes and sends final response
```

## 16. query_result 请求完整时序

```text
client sends query_result frame
  |
  v
ConnectionSlot::read_message
  |
  v
JudgeProtocol::decodeQueryRequest
  |
  v
QueryRequestHandler::handleQuery
  |
  v
SubmissionService::query
  |
  v
ResultStore::getResult
  |
  v
JudgeProtocol::encodeResult
  |
  v
ConnectionSlot queues response
  |
  v
TcpServer sends response
```

如果结果还没完成，返回 `submission_update`。

如果结果已完成，返回 `submission_finished`。

## 17. 读代码时最容易混淆的点

### uuid 和 submission_id

客户端提交里有 `uuid`，但服务端会生成自己的 `submission_id`。

执行阶段会把：

```cpp
execution_request.uuid = submission_id;
```

原因是 runner 用 uuid 作为临时工作目录名。如果直接用客户端 uuid，不同客户端可能互相覆盖。

### ack 和 finished 是两条不同响应

submit 后立即返回：

```text
submission_ack
```

worker 完成后再推：

```text
submission_finished
```

它们之间通过 `reply_channel_id` 和 `AckBarrier` 保证顺序。

### ResultStore 保存的是最新快照

它不是完整历史日志。

每次 `updateResult()` 会覆盖当前 submission 的最新 `SubmissionResult`。

### 网络线程不执行评测

网络线程只入队和回包。真正执行在 worker 线程。

这是系统能同时处理多个连接的关键。

### runner 成功执行不代表 AC

runner 执行成功后还要比较输出。

执行阶段的 `SUCCESS` 只是“程序跑完了”，不是“答案对了”。

## 18. 当前实现限制

理解源码时也要知道当前边界：

1. `SubmissionQueue` 是内存无界队列。
2. `ResultStore` 是内存存储，进程退出结果会丢失。
3. worker 没有任务取消、重试、优先级。
4. 测试点限制目前在 `SubmissionService::load_cases_for_problem()` 里硬编码。
5. 当前只支持 C++ 和 Python，`SubmissionLanguage::C` 是保留值。
6. `ClientSockets` 目前主要主动推送最终结果，不逐测试点推送 running update。
7. 完整沙箱能力依赖 `sjudge/` 后续增强。

## 19. 最小阅读路线

如果你只想快速理解主线，不想一开始读所有细节：

1. `src/main.cpp`
2. `src/common/SubmissionTypes.h`
3. `src/network/ClientSockets.cpp`
   - 看 `handle_complete_message()`
   - 看 `handle_submit_message()`
   - 看 `onSubmissionFinished()`
4. `src/network/ConnectionSlot.cpp`
   - 看 `read_message()`
   - 看 `queue_response_locked()`
5. `src/dispatch/JudgeWorkerPool.cpp`
   - 看 `workerLoop()`
6. `src/pipeline/SubmissionService.cpp`
   - 看 `processSubmission()`
7. `src/runner/RunnerExecutionSupport.cpp`
   - 看 `run_executable_case()`
8. `src/pipeline/ResultStore.cpp`
   - 看 `createSubmission()` / `updateResult()` / `getResult()`

读完这 8 个点，就能把 judge_server 主流程串起来。

