# 给代码小白的文件逐个解释版

这份文档适合你在已经有了“总体路线图”之后使用。

如果你还没看过上一份文档，建议先看：

- `docs/how-to-read-this-project-for-beginners.md`

那一份是“先看哪里、后看哪里”。

这一份是：

**当你真的打开某个文件时，我告诉你这个文件到底该怎么读。**

---

## 一、先记住：你不是在背代码

你读每个文件时，不需要把所有细节都记住。

你只需要回答 4 个问题：

1. 这个文件是干嘛的？
2. 它和谁配合？
3. 你第一次只需要看懂什么？
4. 哪些细节现在可以先跳过？

你可以把这份文档当成“边看边对照的翻译器”。

---

## 二、最值得优先读的文件清单

如果你只读最核心的一批文件，我建议按下面顺序：

1. `README.md`
2. `docs/architecture.md`
3. `main.cpp`
4. `src/TcpServer.cpp`
5. `include/client_sockets.h`
6. `src/client_sockets.cpp`
7. `include/protocol/JudgeProtocol.h`
8. `src/protocol/JudgeProtocol.cpp`
9. `include/service/SubmissionService.h`
10. `src/service/SubmissionService.cpp`
11. `include/runner/ILanguageRunner.h`
12. `src/runner/RunnerFactory.cpp`
13. `src/runner/CppRunner.cpp`
14. `src/runner/PythonRunner.cpp`
15. `src/judge/JudgeCore.cpp`
16. `src/store/ResultStore.cpp`
17. `test/test_submission_service.cpp`
18. `test/test_integration_tcp_cpp_python.cpp`

下面我会一个个解释。

---

## 三、文件逐个解释

## 1. `README.md`

### 这个文件是干嘛的

它是项目说明书。

它告诉你：

- 这个项目做什么
- 现在支持什么语言
- 怎么启动
- 还有哪些重要文档

### 第一次只看什么

你只看：

- “当前能力”
- “快速开始”
- “文档”
- “诚实说明”

### 先别管什么

- 不用纠结每条限制是什么意思
- 不用立刻理解所有模块名

### 读完后你应该知道

**这是一个评测服务，不是普通网站，也不是纯算法代码仓库。**

---

## 2. `docs/architecture.md`

### 这个文件是干嘛的

它给你“全局地图”。

### 第一次只看什么

你只看这些模块名：

- `ClientSockets`
- `JudgeProtocol`
- `SubmissionService`
- `RunnerFactory`
- `JudgeCore`
- `ResultStore`

### 先别管什么

- 不用死抠每一层的实现细节

### 读完后你应该知道

**请求不是直接去跑代码的，而是经过一条清晰流水线。**

---

## 3. `main.cpp`

### 这个文件是干嘛的

它是程序入口。

### 第一次只看什么

你重点看：

- 有没有加载配置
- 有没有启动 TCP server
- 把 socket 事件交给了谁

### 先别管什么

- `std::unique_ptr`
- lambda 写法细节

### 读完后你应该知道

**程序是从这里启动的，但它自己不做判题。**

---

## 4. `src/TcpServer.cpp`

### 这个文件是干嘛的

它负责：

- 监听端口
- 接收连接
- 用 `select` 监控 socket 事件

### 第一次只看什么

你只需要看懂：

- 它负责“网络监听”
- 它不是判题逻辑本身

### 先别管什么

- `select` 的所有参数细节
- 文件描述符的底层原理

### 读完后你应该知道

**这是“前门保安”，负责把请求接进来。**

---

## 5. `include/client_sockets.h`

### 这个文件是干嘛的

这是 socket 管理类的“说明书”。

头文件通常比 `.cpp` 更适合新手先看，因为它告诉你“这个类会做什么”。

### 第一次只看什么

重点看两个类：

- `FdInfo`
- `ClientSockets`

还有这些方法名字：

- `read_message`
- `send`
- `deal_events`
- `add_socket`
- `del_socket`

### 先别管什么

- 锁的细节
- `pending_response_` 这种成员变量的实现细节

### 读完后你应该知道

**每个连接都有自己的状态，`ClientSockets` 负责统一管理它们。**

---

## 6. `src/client_sockets.cpp`

### 这个文件是干嘛的

这是整个“网络接入层”的核心实现。

### 第一次只看什么

你重点找这几段：

- 读事件怎么处理
- 收到消息后怎么交给 `JudgeProtocol`
- 怎么开线程去跑 `SubmissionService`
- 怎么把响应再发回去

### 先别管什么

- `send()` 半写问题
- 输入缓冲区细节
- 老旧 `testBox` 兼容逻辑细节

### 读完后你应该知道

**这个文件完成了“收到请求 -> 送去判题 -> 回包”这条外层链路。**

---

## 7. `include/protocol/JudgeProtocol.h`

### 这个文件是干嘛的

这是协议层接口说明。

### 第一次只看什么

看 3 个方法：

- `decodeRequest`
- `encodeResult`
- `encodeError`

### 读完后你应该知道

**协议层只做翻译，不做业务判断。**

---

## 8. `src/protocol/JudgeProtocol.cpp`

### 这个文件是干嘛的

它把 JSON 请求变成内部结构，又把内部结果变回 JSON。

### 第一次只看什么

你只看：

- `uuid/pid/lang/code` 怎么被读出来
- `status/verdict/message/case_results` 怎么被写回去

### 先别管什么

- 每个 `switch` 的所有分支
- 所有 JSON 细节

### 读完后你应该知道

**协议层负责“听懂人话”和“把程序结果说回去”。**

---

## 9. `include/service/SubmissionService.h`

### 这个文件是干嘛的

它定义了服务层最核心的两个动作：

- `submit()`
- `query()`

### 第一次只看什么

看：

- 它依赖了哪些模块
- 它的接口长什么样

### 读完后你应该知道

**这层是总调度员。**

---

## 10. `src/service/SubmissionService.cpp`

### 这个文件是干嘛的

这是整个项目最值得你重点读的文件之一。

### 第一次只看什么

你只顺着 `submit()` 从上往下读，找出这些阶段：

- 创建 submission
- `PREPARING`
- 选 runner
- `COMPILING`
- `RUNNING`
- 汇总结果
- `FINISHED` 或 `FAILED`

### 先别管什么

- `scan_data_list` 的内部细节
- `filesystem` 的细节
- 具体异常怎么抛出

### 读完后你应该知道

**这里是“整个评测流程真正发生的地方”。**

---

## 11. `include/runner/ILanguageRunner.h`

### 这个文件是干嘛的

它定义所有语言 runner 的共同接口。

### 第一次只看什么

看这几个动作：

- `prepare()`
- `compile()`
- `runCase()`

### 读完后你应该知道

**不管是什么语言，系统都希望用统一方式驱动它。**

---

## 12. `src/runner/RunnerFactory.cpp`

### 这个文件是干嘛的

它负责按语言选择正确的 runner。

### 第一次只看什么

只看：

- `CPP` 对应什么
- `PYTHON` 对应什么
- 不支持的语言会怎么样

### 读完后你应该知道

**系统不是写死一种语言，而是通过工厂选执行器。**

---

## 13. `src/runner/CppRunner.cpp`

### 这个文件是干嘛的

它专门处理 `C++` 提交。

### 第一次只看什么

只看三件事：

- 怎么写源文件
- 怎么编译
- 怎么跑单个测试点

### 先别管什么

- 系统调用细节
- 所有临时文件路径细节

### 读完后你应该知道

**C++ runner = 写代码文件 + 调 g++ + 运行程序。**

---

## 14. `src/runner/PythonRunner.cpp`

### 这个文件是干嘛的

它专门处理 `Python` 提交。

### 第一次只看什么

重点看：

- 怎么写 `solution.py`
- 为什么 `python3 -m py_compile` 被当成 compile-check
- 怎么跑单个测试点

### 读完后你应该知道

**Python 虽然不是传统编译型语言，但这里仍然被放进统一流程里。**

---

## 15. `src/judge/JudgeCore.cpp`

### 这个文件是干嘛的

负责把多个 case 的结果汇总成一个最终 verdict。

### 第一次只看什么

只看 verdict 优先级规则。

### 读完后你应该知道

**这层不执行代码，只负责“最后怎么判”。**

---

## 16. `src/store/ResultStore.cpp`

### 这个文件是干嘛的

负责保存结果和控制状态流转。

### 第一次只看什么

重点看：

- `createSubmission()`
- `updateResult()`
- `getResult()`
- `isValidTransition()`

### 读完后你应该知道

**这层像“进度记录本”，不是执行器。**

---

## 17. `include/common/SubmissionTypes.h`

### 这个文件是干嘛的

它定义了新链路最核心的数据结构。

### 第一次只看什么

看这几个结构：

- `SubmissionRequest`
- `SubmissionCaseResult`
- `SubmissionResult`

还有这些枚举：

- `SubmissionStatus`
- `SubmissionLanguage`
- `SubmissionVerdict`

### 读完后你应该知道

**这些结构就是新判题链路里的“共同语言”。**

---

## 18. `test/test_submission_service.cpp`

### 这个文件是干嘛的

它在测试整个服务层是不是按预期工作。

### 第一次只看什么

看：

- 成功路径怎么测
- 编译失败怎么测
- 不支持语言怎么测

### 读完后你应该知道

**测试其实是在告诉你：服务层作者认为“正确行为”应该是什么。**

---

## 19. `test/test_integration_tcp_cpp_python.cpp`

### 这个文件是干嘛的

它在做 TCP 端到端集成测试。

### 第一次只看什么

看：

- 请求怎么发进去
- 返回 JSON 长什么样
- `C++` 和 `Python` 是否都能走通

### 读完后你应该知道

**这份测试最像真实使用场景。**

---

## 四、第一次看文件时的通用技巧

你每打开一个新文件，都先做这 3 件事：

### 1. 先看文件名

文件名通常已经在告诉你职责。

比如：

- `JudgeProtocol` = 协议翻译
- `SubmissionService` = 提交流程服务
- `ResultStore` = 结果存储

### 2. 先看类名和函数名

不要先看函数体。

先看“有哪些动作”。

### 3. 先找主流程，再看 helper

主流程通常是：

- `submit()`
- `runCase()`
- `decodeRequest()`
- `encodeResult()`

helper 小函数第一次可以先略过。

---

## 五、如果你读某个文件很难受，正常

下面这些文件，对小白来说本来就更难：

- `src/client_sockets.cpp`
- `src/test_process/TestOneSinglePoint.cpp`
- `src/test_process/Compile.cpp`
- `Buffer` 相关代码

原因不是你笨，而是这些文件：

- 更靠近系统底层
- 有历史兼容逻辑
- 有 socket / 线程 / 进程 / 文件系统细节

所以你第一次读这些文件时，只要先搞清楚“它大概负责什么”，就已经够了。

---

## 六、你读完这一轮之后，应该能回答的问题

如果你把上面这些文件按顺序读完，你应该已经能回答：

1. 代码提交是从哪里进来的？
2. JSON 请求是谁解析的？
3. 整个评测流程是谁调度的？
4. `C++` 和 `Python` 分别是谁执行的？
5. 最终 verdict 是谁决定的？
6. 结果保存在哪里？
7. 为什么这个项目能扩展更多语言？

如果这 7 个问题你能说出大概意思，你已经不是“完全看不懂项目的人”了。

---

## 七、最后给你的建议

不要要求自己第一次就看懂所有底层细节。

你现在更应该追求的是：

**看到一个文件时，能说出它在整个系统里的位置。**

这比背语法更重要。

接下来如果你愿意，我就继续给你写第 2 份：

- `给小白的核心数据结构解释版`

那一份会专门讲：

- `SubmissionRequest`
- `SubmissionResult`
- `SubmissionCaseResult`
- `RunnerCaseInput`
- `RunnerCompileResult`
- `RunnerCaseResult`

这些结构到底装了什么、为什么要这样设计。
