# 测试说明

## 当前测试覆盖

CTest 里已经注册了 `test/test*.cpp` 下的自动化测试，重点覆盖：

- `test/test_protocol_codec.cpp`：协议 JSON 编解码
- `test/test_result_store.cpp`：结果存储与状态机
- `test/test_judge_core.cpp`：verdict 汇总优先级
- `test/test_runner_factory.cpp`：语言到 runner 的映射
- `test/test_cpp_runner.cpp`：C++ prepare/compile/run
- `test/test_python_runner.cpp`：Python prepare/compile/run
- `test/test_submission_service.cpp`：服务层完整编排
- `test/test_integration_tcp_cpp_python.cpp`：TCP framed JSON 端到端链路

旧的手工调试程序，如 `test/main.cpp`、`test/send_one_test.cpp`、`test/2.cpp`，不会注册进 CTest。

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

### 跑单个测试

```bash
ctest --test-dir build -R '^test_protocol_codec$' --output-on-failure
ctest --test-dir build -R '^test_submission_service$' --output-on-failure
ctest --test-dir build -R '^test_integration_tcp_cpp_python$' --output-on-failure
```

### 直接运行某个测试可执行文件

```bash
./build/test_protocol_codec
./build/test_submission_service
```

如果 CTest 因目录迁移保留了旧绝对路径，重新执行一次 `cmake -S . -B build` 即可刷新。

## Node 脚本现状

`test/nodejs/` 可用于协议验证和手工联调，但现状需要注意：

- npm scripts 仍指向旧入口：`test_codec.js`、`demo.js`、`send_one_judge.js`。
- 目录内文档提到的新脚本名与当前 `package.json` 并不完全一致，说明这部分仍在迁移中。
- `lib/data-codec.js` 仍保留旧二进制结构编解码函数，但也新增了 `serializeTestProblemToJsonString()` 用于 JSON 请求。

可用命令：

```bash
cd test/nodejs
npm install
npm test
npm run demo
npm run judge
```

如果要验证当前主线协议，优先看 `test/test_protocol_codec.cpp` 和 `test/test_integration_tcp_cpp_python.cpp`，它们最贴近现在服务端实现。

## 测试依赖

- C++ runner 依赖本机可用的 `g++`。
- Python runner 依赖 `python3`。
- 端到端测试依赖仓库内 `testData/1000/data`。
- `/usr/bin/sjudge` 与 `/judge/checker/fcmp2` 不是测试必需；没有它们时，测试会走 fallback 路径。

## 已知边界

- fallback 路径只保证真实时间超时，不验证真正 CPU/内存隔离，因此与生产部署装好 sjudge 后的行为可能略有差异。
- 当前端到端测试验证的是“一次请求拿到最终 JSON 响应”，并不覆盖未来可能出现的轮询/流式状态接口。
