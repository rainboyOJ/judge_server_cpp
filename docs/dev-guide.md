# 开发指南

## 先理解当前代码形态

这个 worktree 不是从零开始的新服务，而是“在旧工程上逐步抽出新判题主链路”的状态。因此开发时建议先分清两类模块：

- 新链路：`protocol/`、`service/`、`runner/`、`judge/`、`store/`
- 旧基础设施与兼容层：`testBox`、`resultContainer`、`client_sockets`、`test_process/`

如果改动的是提交流程、协议、runner 或结果模型，优先沿着新链路实现；除非是兼容性需要，不要把新逻辑再塞回旧 `testBox` 流程里。

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
ctest --test-dir build -R '^test_cpp_runner$' --output-on-failure
ctest --test-dir build -R '^test_python_runner$' --output-on-failure
ctest --test-dir build -R '^test_integration_tcp_cpp_python$' --output-on-failure
```

### 3. 再跑全量

```bash
ctest --test-dir build --output-on-failure
```

## 模块改动建议

### 协议相关

改这些文件：

- `include/common/SubmissionTypes.h`
- `include/protocol/JudgeProtocol.h`
- `src/protocol/JudgeProtocol.cpp`
- `test/test_protocol_codec.cpp`
- 如影响联调，再看 `test/test_integration_tcp_cpp_python.cpp`

### 服务流程相关

改这些文件：

- `include/service/SubmissionService.h`
- `src/service/SubmissionService.cpp`
- `include/store/ResultStore.h`
- `src/store/ResultStore.cpp`
- `include/judge/JudgeCore.h`
- `src/judge/JudgeCore.cpp`
- `test/test_submission_service.cpp`
- `test/test_result_store.cpp`
- `test/test_judge_core.cpp`

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
- 新逻辑优先复用现有 `SubmissionRequest` / `SubmissionResult` / `RunnerCaseResult`。
- 错误信息保持短而可定位，例如 `unsupported language`、`failed to load problem cases`。
- 运行期状态必须遵守 `ResultStore` 的前进式状态机。

## 调试技巧

- Debug 构建会定义 `MUDEBUG`，可启用 `LOG_DEBUG`。
- TCP framing 问题优先看 `FdInfo::read_message()` 和 `set_pending_response()`。
- 结果异常优先检查 `SubmissionService::submit()` 是否提前写成 `FAILED/FINISHED`。
- 判题结果不符合预期时，先区分是执行问题、checker 问题，还是 `JudgeCore` 汇总问题。

## 当前要诚实面对的坑

- `ClientSockets` 新链路回包虽然异步触发，但内部用了 `detach` 线程；如果未来要做高并发或可控关闭，这里需要更正式的任务管理。
- `Buffer.cpp`、`client_sockets.cpp` 等旧网络代码仍比较脆弱，改动前最好先补测试或最少做 socketpair 级验证。
- Node 目录仍有新旧脚本并存现象，改协议时不要只改一边文档或一边脚本。
- 当前题目限制没有配置化；如果要做真正的题目模型扩展，不能只在 runner 里硬编码。
