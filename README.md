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
- `docs/完整评测的流程.md`：一次 submit 从 TCP 请求到最终结果回推的完整链路
- `docs/protocol.md`：TCP framing、JSON 结构、状态与判题结果
- `docs/runner-extension.md`：如何新增一种语言 runner
- `docs/testing.md`：CTest、单测、Node 脚本与端到端验证
- `docs/deployment-linux.md`：Linux 构建、运行、checker/sjudge 部署
- `docs/dev-guide.md`：开发者工作流与改动建议

## 代码结构

下面以 `src/` 为主，按 tree 风格列出当前核心代码结构，并在每个文件后面附一行简要说明。

```text
src/
├── main.cpp                                  # 程序入口，组装服务、网络层和 worker pool
├── json.hpp                                  # vendored 的 nlohmann/json 单头文件库
├── common/
│   ├── Config.h                              # 配置读取接口
│   ├── Config.cpp                            # 配置文件加载与配置项访问实现
│   ├── Logger.h                              # 日志宏与日志接口定义
│   ├── Logger.cpp                            # 日志输出实现
│   ├── Result.h                              # 旧结果结构定义，供兼容代码使用
│   ├── SubmissionTypes.h                     # submission 请求、状态、verdict 等核心共享类型
│   ├── Timestamp.h                           # 时间戳工具接口
│   ├── Timestamp.cpp                         # 时间戳工具实现
│   ├── Types.h                               # 旧基础类型定义，保留给兼容路径
│   ├── noncopyable.h                         # 禁止拷贝的基类工具
│   ├── utils.h                               # 通用辅助函数声明
│   └── utils.cpp                             # 通用辅助函数实现，如扫描测试数据文件
├── dispatch/
│   ├── JudgeWorkerPool.h                     # 后台评测线程池接口
│   ├── JudgeWorkerPool.cpp                   # worker 线程从 SubmissionService 取任务并驱动评测
│   ├── SubmissionNotifier.h                  # worker 生命周期通知接口
│   ├── SubmissionQueue.h                     # SubmissionService 内部使用的线程安全阻塞队列
│   ├── SubmissionQueue.cpp                   # submission 队列实现
│   └── SubmissionTask.h                      # 后台 worker 流转的任务结构
├── network/
│   ├── AckBarrier.h                          # ack 与后续异步消息顺序保护器接口
│   ├── AckBarrier.cpp                        # ack 屏障实现，负责 defer/release 协议消息
│   ├── Buffer.h                              # 网络读缓冲接口
│   ├── Buffer.cpp                            # 网络读缓冲实现
│   ├── ClientSockets.h                       # 客户端连接管理与异步回推入口
│   ├── ClientSockets.cpp                     # 连接事件分发、submit/query 路由与结果回包实现
│   ├── ConnectionRegistry.h                  # 连接槽位表接口
│   ├── ConnectionRegistry.cpp                # 连接槽位分配、释放和 fd_set 参与逻辑实现
│   ├── ConnectionSlot.h                      # 单连接槽位状态接口
│   ├── ConnectionSlot.cpp                    # 单连接的读写缓冲、session 校验和待发送响应实现
│   ├── QueryRequestHandler.h                 # `query_result` 请求处理器接口
│   ├── QueryRequestHandler.cpp               # query 命中/未命中查询逻辑实现
│   ├── ReplyChannel.h                        # reply channel 值对象接口
│   ├── ReplyChannel.cpp                      # reply channel 的 parse/format 实现
│   ├── SubmissionEventResponder.h            # worker 事件转协议消息的接口
│   ├── SubmissionEventResponder.cpp          # started/finished 事件到协议响应的翻译实现
│   ├── SubmissionRequestHandler.h            # submit 请求处理器接口
│   ├── SubmissionRequestHandler.cpp          # submit 调用 SubmissionService、ack release 逻辑实现
│   ├── TcpServer.h                           # 基于 select 的 TCP server 接口
│   └── TcpServer.cpp                         # accept/select/eventfd 主循环实现
├── pipeline/
│   ├── SubmissionVerdictReducer.h                           # 判题结果归并接口
│   ├── SubmissionVerdictReducer.cpp                         # 多个 case verdict 汇总实现
│   ├── ResultStore.h                         # submission 快照存储接口
│   ├── ResultStore.cpp                       # 线程安全结果存储与惰性清理实现
│   ├── SubmissionService.h                   # submission 编排服务接口
│   └── SubmissionService.cpp                 # 建单、内部入队、状态推进、runner 调用与结果持久化实现
├── protocol/
│   ├── JudgeProtocol.h                       # JSON 协议编解码接口
│   └── JudgeProtocol.cpp                     # submit/query 请求与结果响应编码实现
├── runner/
│   ├── CppRunner.h                           # C++ runner 接口定义
│   ├── CppRunner.cpp                         # C++ 提交 prepare/compile/runCase 实现
│   ├── ILanguageRunner.h                     # 统一语言 runner 抽象接口
│   ├── PythonRunner.h                        # Python runner 接口定义
│   ├── PythonRunner.cpp                      # Python 提交 prepare/compile/runCase 实现
│   ├── RunnerSupport.h                       # runner 共用支撑函数声明入口
│   ├── RunnerCompileSupport.cpp              # C++ 编译辅助能力实现
│   ├── RunnerExecutionSupport.cpp            # 可执行程序运行与输出比较辅助实现
│   ├── RunnerFactory.h                       # runner 工厂接口
│   └── RunnerFactory.cpp                     # 按语言类型创建 runner 的实现
```

## 诚实说明

- `SubmissionQueue` 是 `SubmissionService` 的内部实现细节；网络层只和 `SubmissionService` 交互。
- 如果机器上没有 `/usr/bin/sjudge`，会走 fallback 执行路径：只强制真实时间限制，不做真正的 CPU/内存隔离；`cpu_time_ms` 直接复用 `real_time_ms`。
- 输出比较优先调用 `/judge/checker/fcmp2`；如果没有部署 checker，会退化为内置的按行比较（忽略行尾空白和末尾空行）。
- `test/nodejs/` 下既有新的 JSON 协议库，也保留了旧脚本和旧命名；`package.json` 的 npm scripts 目前仍指向旧入口，需要按文档手动选择脚本。
