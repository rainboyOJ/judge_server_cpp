# 架构说明

## 概览

当前实现是一个“旧 TCP 外壳 + 新判题内核”的过渡架构：网络接入仍基于 `select` 和 `ClientSockets`，但请求进入后会转换成统一的 `SubmissionRequest`，再交给 `SubmissionService`、`RunnerFactory`、`JudgeCore`、`ResultStore` 组成的新链路处理。

核心入口与层次：

- `main.cpp`：读取 `config/config.json`，启动 `TcpServer`，把连接事件交给 `ClientSockets`。
- `src/TcpServer.cpp`：维护监听 socket 和 `select` 循环。
- `src/client_sockets.cpp`：做长度前缀收包、协议解码、线程切换、回包。
- `src/protocol/JudgeProtocol.cpp`：JSON <-> 内部结构转换。
- `src/service/SubmissionService.cpp`：编排一次完整提交的状态推进。
- `src/runner/*.cpp`：语言相关准备、编译、单点运行。
- `src/judge/JudgeCore.cpp`：把多个测试点结果归并成最终 verdict。
- `src/store/ResultStore.cpp`：线程安全保存提交快照，并限制状态只能前进。
- `src/test_process/*.cpp`：从旧流程抽出的可复用能力，如 C++ 编译、执行、比较输出。

## 分层

### 1. 网络接入层

`TcpServer` 负责监听与 `select`，`ClientSockets` 负责每个 fd 的读写状态：

- 读：从 `FdInfo::input_buffer_` 中累积数据。
- 解帧：读取前 `4` 字节大端长度，再拿到完整消息体。
- 写：响应同样加 `4` 字节长度前缀，支持部分写出后的续传。

这一层目前仍保留旧 `testBox` 生命周期管理能力，因此是兼容层，不是完全重写后的纯净网络模块。

### 2. 协议层

`JudgeProtocol` 只做协议转换，不负责编排：

- `decodeRequest()` 解析旧客户端沿用的 JSON 字段。
- `encodeResult()` 输出统一结果 JSON。
- `encodeError()` 输出兼容错误包。

协议层已经不再复用旧 `testProblem` 的固定内存布局，因此 `pid` 不再受旧的短字符串限制。

### 3. 服务编排层

`SubmissionService` 是新链路的核心：

1. 在 `ResultStore` 中创建 `submission_id`，初始状态 `QUEUED`。
2. 写入 `PREPARING`。
3. 用 `RunnerFactory` 选择 runner。
4. 调 `prepare()` 建工作目录和源码文件。
5. 写入 `COMPILING`，再调 `compile()`。
6. 若编译失败，直接以 `FINISHED + CE` 结束。
7. 成功后写入 `RUNNING`，加载题目数据并逐点 `runCase()`。
8. 每跑完一个点就回写一次快照。
9. 最后调用 `JudgeCore::summarize()` 生成最终 verdict，并写入 `FINISHED`。

注意：这里虽然从 socket 线程切到了一个独立线程，但 `submit()` 内部是同步串行跑完整次评测，然后才准备响应，所以当前返回的是“最终结果”，不是流式中间状态。

### 4. 执行层

`ILanguageRunner` 定义三个动作：

- `prepare()`：准备工作目录与源码。
- `compile()`：编译或语法检查。
- `runCase()`：执行单个测试点。

当前实现：

- `CppRunner`：写 `solution.cpp`，调用 `g++ -std=c++17 -O2 -DONLINE_JUDGE` 编译，生成可执行文件。
- `PythonRunner`：写 `solution.py`，通过 `python3 -m py_compile` 做语法检查，再用一个临时 shell wrapper 执行。

### 5. 判题与存储层

- `JudgeCore`：只关心 verdict 优先级归并，不关心语言和协议。
- `ResultStore`：用互斥锁保护 `unordered_map`，并限制状态只能按 `QUEUED -> PREPARING -> COMPILING -> RUNNING -> FINISHED/FAILED` 前进。

## 请求生命周期

```text
client
  -> TCP framed JSON
  -> ClientSockets::read_message
  -> JudgeProtocol::decodeRequest
  -> detached worker thread
  -> SubmissionService::submit
  -> ResultStore / Runner / JudgeCore
  -> JudgeProtocol::encodeResult
  -> ClientSockets write path
  -> TCP framed JSON response
```

## 与旧系统的边界

当前代码不是完全替换旧 `testBox`：

- `main.cpp` 仍创建 `testBox`，`ClientSockets` 也仍依赖它分配 `testBoxId`。
- 新 JSON 请求优先走 `SubmissionService` 新链路。
- 写回路径保留旧 `testBox_->getResult()` fallback，方便尚未迁移的旧调用链继续工作。

这意味着现在的系统是“新老并存”的过渡态，文档与开发时都应按此理解，不宜假设旧模块已经完全下线。

## 当前限制

- 真正的异步队列、任务取消、结果轮询接口都还没有；当前一次连接通常对应一次提交和一次最终响应。
- `RunnerFactory` 只支持 C++/Python；`SubmissionLanguage::C` 仅是保留值。
- 测试点时间/内存限制目前硬编码在 `SubmissionService::load_cases_for_problem()`，尚未从题目配置读取。
- `client_sockets_` 的槽位大小仍依赖 `testBox->size()`，连接容量管理沿用旧思路。
