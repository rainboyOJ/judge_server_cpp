# 测试说明

## 当前测试覆盖

CTest 当前重点覆盖同步判题内核和新的异步调度链路：

- `test/test_protocol_codec.cpp`：`submit` / `query_result` 解码，以及 `submission_ack` / `submission_update` / `submission_finished` 编码形状。
- `test/test_result_store.cpp`：`createSubmission()`、快照读写、状态机前进约束。
- `test/test_judge_core.cpp`：verdict 汇总优先级。
- `test/test_runner_factory.cpp`：语言到 runner 的映射。
- `test/test_cpp_runner.cpp`：C++ `prepare/compile/run`。
- `test/test_python_runner.cpp`：Python `prepare/compile/run`。
- `test/test_submission_service.cpp`：`SubmissionService::processSubmission()` 的完整编排。
- `test/test_judge_worker_pool.cpp`：`SubmissionQueue` + `JudgeWorkerPool` + `SubmissionNotifier` 协作。
- `test/test_async_submission_flow.cpp`：异步 ack 后通过 `query_result` 拿当前或最终快照，以及断线后恢复最终结果。
- `test/test_integration_tcp_cpp_python.cpp`：TCP framed JSON 端到端链路，验证"先 ack、后 finished 推送"。
- `test/test_submission_queue.cpp`：`SubmissionQueue` push/pop/shutdown 行为。
- `test/test_connection_registry.cpp`：连接注册表单元测试。
- `test/test_tcp_server_eventfd.cpp`：TcpServer eventfd 唤醒机制。
- `test/test_json_simple.cpp`：简单 JSON 解析。
- `test/test_submission_types_compile.cpp`：SubmissionTypes 编译期检查。
- `test/test_static_loop_queue.cpp`：legacy static_loop_queue。
- `test/testMemPool.cpp`：legacy memPool。
- `test/testJudgeInfoSerialize.cpp`：legacy judgeInfo 序列化。

旧的手工调试程序（`test/main.cpp`、`test/send_one_test.cpp`、`test/2`）不会注册进 CTest。

## 常用命令

### 配置和构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

### 跑全部测试

```bash
ctest --test-dir build --output-on-failure
```

### 跑异步主链路相关测试

```bash
ctest --test-dir build -R '^test_protocol_codec$' --output-on-failure
ctest --test-dir build -R '^test_submission_service$' --output-on-failure
ctest --test-dir build -R '^test_judge_worker_pool$' --output-on-failure
ctest --test-dir build -R '^test_async_submission_flow$' --output-on-failure
ctest --test-dir build -R '^test_integration_tcp_cpp_python$' --output-on-failure
```

### 跑 runner 相关测试

```bash
ctest --test-dir build -R '^test_(cpp|python)_runner$' --output-on-failure
ctest --test-dir build -R '^test_runner_factory$' --output-on-failure
```

### 直接运行某个测试可执行文件

```bash
./build/test/test_protocol_codec
./build/test/test_submission_service
./build/test/test_async_submission_flow
```

如果 CTest 因目录迁移保留了旧绝对路径，重新执行一次 `cmake -S . -B build` 即可刷新。

## Node 脚本现状

`test/nodejs/` 目前仍主要验证 framing / codec 辅助逻辑，不直接覆盖完整异步后端：

- `npm test` 实际执行的是 `node test_codec.js`。
- `npm run demo` 是手工联调演示脚本。
- `npm run judge` 是旧同步评测脚本。
- `npm run async-judge` 是异步评测演示脚本 `send_async_judge.js`。

目录里同时保留旧二进制结构兼容代码和新的 JSON 辅助函数，说明 Node 端也处于迁移期。

```bash
cd test/nodejs
npm install
npm test
npm run demo
npm run async-judge
```

如果要验证现在的 async 主线，优先看 C++ 测试，尤其是：

- `test/test_protocol_codec.cpp`
- `test/test_judge_worker_pool.cpp`
- `test/test_async_submission_flow.cpp`
- `test/test_integration_tcp_cpp_python.cpp`

## 测试依赖

- C++ runner 依赖本机可用的 `g++`。
- Python runner 依赖 `python3`。
- 端到端测试依赖仓库内 `testData/1000/data`。
- `/usr/bin/sjudge` 与 `/judge/checker/fcmp2` 不是测试必需；没有它们时，测试会走 fallback 路径。

## 已知边界

- 目前自动化测试已经覆盖 async ack、后台 worker 和 `query_result`，但还没有"逐状态主动推送 `submission_update`"的端到端测试，因为当前实现本身没有这条完整推送链路。
- `query_result` 测的是"读取 ResultStore 当前快照"，不是阻塞等待接口。
- fallback 路径只保证真实时间超时，不验证真正 CPU/内存隔离，因此与装好 sjudge 后的生产行为可能略有差异。
- Node `npm test` 只验证 Node 侧 codec/framing 辅助逻辑，不能替代 CTest 的 async integration coverage。
