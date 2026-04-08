# 扩展新的语言 Runner

## 当前接口

新语言必须实现 `include/runner/ILanguageRunner.h`：

- `prepare(const SubmissionRequest&)`
- `compile(const SubmissionRequest&)`
- `runCase(const SubmissionRequest&, const RunnerCaseInput&)`

服务层只依赖这个接口，因此新增语言时不需要改协议层和 `SubmissionService` 的主流程。

## 当前已实现示例

- `src/runner/CppRunner.cpp`
- `src/runner/PythonRunner.cpp`

这两个文件基本展示了新增 runner 所需的全部结构：

- 约定工作目录：`/tmp/oj_compile_<submission_id>`
- 在 `prepare()` 写入源码文件
- 在 `compile()` 做真正编译或语法检查
- 在 `runCase()` 复用 `run_executable_case()` 统一执行与判题

## 新增步骤

### 1. 扩充语言枚举

如果是全新语言值，先修改 `include/common/SubmissionTypes.h` 的 `SubmissionLanguage`。

注意兼容性：`lang` 是协议对外字段，改动后要同步更新：

- `src/protocol/JudgeProtocol.cpp` 的 `toSubmissionLanguage()`
- Node 客户端常量 `test/nodejs/lib/constants.js`
- 文档 `docs/protocol.md`

### 2. 新建 runner 文件

典型文件：

- `include/runner/YourLanguageRunner.h`
- `src/runner/YourLanguageRunner.cpp`

建议直接仿照 `CppRunner` / `PythonRunner`：

- 统一复用 `work_dir_for(request)` 风格
- 统一把 `request.uuid` 视为工作目录键
- 错误时返回 `SYSTEM_ERROR` 或 `CE`
- 单点执行尽量接入 `run_executable_case()`，避免再复制一套执行/比较逻辑

### 3. 接入工厂

修改：

- `include/runner/RunnerFactory.h`
- `src/runner/RunnerFactory.cpp`
- 根 `CMakeLists.txt`

确保新增源文件被静态库 `boxTest` 编译进去，并让 `RunnerFactory::createRunner()` 返回对应实例。

### 4. 明确编译阶段语义

当前服务层假设所有语言都有统一的 `COMPILING` 阶段：

- C++：真实编译
- Python：`py_compile` 语法检查

因此新增解释型语言时，也建议在 `compile()` 里做最小合法性检查，而不是直接返回空实现。这样能保证状态机和前端心智一致。

### 5. 补测试

至少补这些测试：

- 新建 `test/test_<language>_runner.cpp`
- 更新 `test/test_runner_factory.cpp`
- 如协议枚举变化，补/改 `test/test_protocol_codec.cpp`
- 如需端到端验证，扩展 `test/test_integration_tcp_cpp_python.cpp`

## 推荐实现模式

### 编译型语言

- `prepare()`：写源码文件。
- `compile()`：调用外部编译器，失败时返回 `CE`。
- `runCase()`：直接执行编译产物。

### 解释型语言

- `prepare()`：写脚本文件。
- `compile()`：做语法检查或预编译。
- `runCase()`：生成轻量 wrapper，再交给统一执行器。

## 当前限制与注意事项

- 当前工作目录在系统临时目录下，runner 需要自己避免路径冲突；服务层已把客户端 `uuid` 重写成 `submission_id` 来减小冲突。
- fallback 执行路径没有真正资源隔离，新语言如果运行时特别重，测试与文档要明确说明。
- `run_executable_case()` 目前默认比较标准输出，不支持交互题、特殊判题器参数注入、多个输出文件等高级模式。
- `SubmissionService::load_cases_for_problem()` 还没有从题目配置读取语言差异或限制差异，因此新 runner 当前默认沿用统一的 1000ms / 1024*1024KB 设置。
