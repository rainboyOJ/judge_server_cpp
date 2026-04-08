# 给代码小白的函数调用链解释版

这份文档是为一种很常见的痛苦写的：

**你明明已经打开代码了，但函数一个跳一个，越跳越晕。**

所以这份文档不重点讲“模块是什么”。

它重点讲：

**“谁调用谁，调用顺序是什么。”**

你可以把它理解成一份“路线导航图”。

---

## 一、先记住：函数调用链不是让你背的

你不需要背住所有函数名。

你只需要先记住主线：

```text
main()
  -> TcpServer
  -> ClientSockets::deal_events()
  -> JudgeProtocol::decodeRequest()
  -> SubmissionService::submit()
  -> RunnerFactory::createRunner()
  -> runner->prepare()
  -> runner->compile()
  -> runner->runCase()
  -> JudgeCore::summarize()
  -> ResultStore::updateResult()
  -> JudgeProtocol::encodeResult()
  -> ClientSockets 回包
```

如果这条线你能大概讲出来，说明你已经抓住骨架了。

---

## 二、从最外层开始：`main()`

文件：

- `main.cpp`

最重要的调用链是：

```text
main()
  -> Config::getInstance().loadFromFile()
  -> 创建 testBox
  -> 创建 ClientSockets
  -> 创建 TcpServer
  -> server.start()
```

### 这一步你要怎么理解

`main()` 不负责判题细节。

它只负责：

- 读配置
- 把各个对象创建好
- 把 TCP server 跑起来

所以 `main()` 的角色更像：

**开机和布置现场。**

---

## 三、请求真正进来的地方：`TcpServer`

文件：

- `src/TcpServer.cpp`

你可以先不研究底层 socket。

只记住：

```text
server.start()
  -> 等待连接
  -> 有新连接时调用 add_socket 回调
  -> 有读写事件时调用 deal_events 回调
```

这里已经把最重要的工作交给了：

- `ClientSockets`

所以从阅读主线来说，真正值得你继续跟的是：

- `ClientSockets::deal_events()`

---

## 四、最关键的外层函数：`ClientSockets::deal_events()`

文件：

- `src/client_sockets.cpp`

这是外层调用链里最重要的函数之一。

因为它决定：

- 收到消息之后怎么处理
- 响应准备好之后怎么发回去

---

## 五、当收到读事件时，调用链怎么走

你可以把读事件主线想成：

```text
ClientSockets::deal_events()
  -> FdInfo::read_message()
  -> JudgeProtocol::decodeRequest()
  -> 开后台线程
  -> SubmissionService::submit()
  -> SubmissionService::query()
  -> JudgeProtocol::encodeResult() / encodeError()
  -> FdInfo::set_pending_response_if_fd()
```

我们拆开看。

---

## 六、第一小段：先把消息读完整

调用链：

```text
ClientSockets::deal_events()
  -> FdInfo::read_message()
```

### 这里在做什么

`FdInfo::read_message()` 做的是：

- 从 socket 读数据
- 先看前 4 个字节长度
- 再取出完整消息体

所以这一段的任务不是判题，而是：

**“先把整条请求安全收全。”**

---

## 七、第二小段：把 JSON 请求解析成内部对象

调用链：

```text
ClientSockets::deal_events()
  -> JudgeProtocol::decodeRequest(message_body, request)
```

### 这里在做什么

它把网络层的 JSON 文本，变成：

- `SubmissionRequest`

也就是内部统一用的请求结构。

所以这里你要记住：

**网络消息到这里才第一次变成“业务对象”。**

---

## 八、第三小段：为什么这里要开线程

调用链：

```text
ClientSockets::deal_events()
  -> std::thread(... SubmissionService::submit(...) ...)
```

### 为什么这样做

因为判题过程很慢。

比如：

- 编译
- 跑测试点
- 等输出

如果这些都堵在 `select` 主循环里，其他连接就没法及时处理。

所以这里会把真正的判题工作丢到后台线程。

你可以理解成：

**前台接待先把任务转给后台老师去批改。**

---

## 九、第四小段：真正的核心调用 —— `SubmissionService::submit()`

调用链：

```text
ClientSockets::deal_events()
  -> SubmissionService::submit(request)
```

这一步是整个系统的主轴。

你可以把它看成“总调度函数”。

它内部的调用链大概是：

```text
SubmissionService::submit()
  -> ResultStore::createSubmission()
  -> ResultStore::updateResult(PREPARING)
  -> RunnerFactory::createRunner()
  -> runner->prepare()
  -> ResultStore::updateResult(COMPILING)
  -> runner->compile()
  -> ResultStore::updateResult(RUNNING)
  -> load_cases_for_problem()
  -> runner->runCase()  [多次]
  -> ResultStore::updateResult(运行中快照)
  -> JudgeCore::summarize()
  -> ResultStore::updateResult(FINISHED)
```

这就是整个评测主调用链。

---

## 十、先看第一步：创建 submission

调用链：

```text
SubmissionService::submit()
  -> ResultStore::createSubmission(request)
```

### 这里在做什么

生成一个：

- `submission_id`

并在存储里创建一份初始记录。

这是系统内部真正用来追踪这次提交的 id。

---

## 十一、再看第二步：更新状态

调用链：

```text
SubmissionService::submit()
  -> ResultStore::updateResult(...)
```

这个函数会被反复调用。

因为系统每推进一步，都要把当前状态写下来。

比如：

- `PREPARING`
- `COMPILING`
- `RUNNING`
- `FINISHED`

所以你要把 `ResultStore::updateResult()` 理解成：

**“每走一步，就记一笔。”**

---

## 十二、接着看第三步：选 runner

调用链：

```text
SubmissionService::submit()
  -> RunnerFactory::createRunner(request.language)
```

### 这里在做什么

它根据语言决定交给谁。

再往下就是：

```text
RunnerFactory::createRunner()
  -> new CppRunner()
或
  -> new PythonRunner()
```

也就是说：

**总调度员不自己执行代码，而是找对应语言的执行器。**

---

## 十三、runner 内部调用链怎么理解

### 对所有语言都一样的外形

调用链：

```text
runner->prepare()
runner->compile()
runner->runCase()
```

这三个动作就是 runner 的统一接口。

### 你先把它们翻译成人话

- `prepare()` = 先把工作环境准备好
- `compile()` = 先检查/编译代码
- `runCase()` = 跑一个测试点

---

## 十四、`CppRunner` 的典型调用链

调用链：

```text
SubmissionService::submit()
  -> CppRunner::prepare()
  -> CppRunner::compile()
  -> CppRunner::runCase()
  -> run_executable_case()
```

### 它在做什么

- `prepare()`：写 `solution.cpp`
- `compile()`：调 `g++`
- `runCase()`：跑可执行文件

所以 `CppRunner` 的核心逻辑非常朴素：

**写代码文件 -> 编译 -> 执行**

---

## 十五、`PythonRunner` 的典型调用链

调用链：

```text
SubmissionService::submit()
  -> PythonRunner::prepare()
  -> PythonRunner::compile()
  -> PythonRunner::runCase()
  -> run_executable_case()
```

### 它在做什么

- `prepare()`：写 `solution.py`
- `compile()`：用 `python3 -m py_compile` 做语法检查
- `runCase()`：执行 Python 脚本

注意：

虽然 Python 不是传统编译型语言，但这里还是故意让它走 `compile()` 这一步。

这样服务层就不用因为语言不同而写两套流程。

---

## 十六、跑完所有测试点之后，谁来决定最终 verdict

调用链：

```text
SubmissionService::submit()
  -> JudgeCore::summarize(running_result.case_results)
```

### 这里在做什么

`JudgeCore` 不执行代码。

它只做一件事：

**根据所有 case 的结果，得出最后总结果。**

比如：

- 全 AC -> AC
- 有 CE -> CE
- 有 TLE -> TLE

所以这一层更像“总评老师”。

---

## 十七、结果怎么回到客户端

后台线程跑完之后，调用链会变成：

```text
SubmissionService::submit()
  -> SubmissionService::query()
  -> JudgeProtocol::encodeResult()
  -> FdInfo::set_pending_response_if_fd()
```

这一步做的是：

- 从 `ResultStore` 取出结果
- 编码成 JSON
- 放到 socket 的待发送缓冲区

然后主循环下一次遇到写事件，就会继续：

```text
ClientSockets::deal_events()
  -> FdInfo::view_pending_response()
  -> FdInfo::send()
  -> FdInfo::consume_pending_response()
```

最后，客户端收到结果。

---

## 十八、最短主调用链版本

如果你只想记最短版本，就记这条：

```text
main()
  -> TcpServer::start()
  -> ClientSockets::deal_events()
  -> FdInfo::read_message()
  -> JudgeProtocol::decodeRequest()
  -> SubmissionService::submit()
  -> RunnerFactory::createRunner()
  -> runner->prepare()
  -> runner->compile()
  -> runner->runCase()
  -> JudgeCore::summarize()
  -> ResultStore::updateResult()
  -> SubmissionService::query()
  -> JudgeProtocol::encodeResult()
  -> FdInfo::send()
```

这条链就是这个项目最核心的调用主线。

---

## 十九、如果你读调用链时总是迷路，怎么办

这里有个非常实用的办法。

你拿一张纸，只写这种格式：

```text
谁调用了谁
谁又调用了谁
最后结果交给谁
```

比如：

```text
ClientSockets 收到请求
-> JudgeProtocol 解析
-> SubmissionService 调度
-> Runner 执行
-> JudgeCore 汇总
-> ResultStore 保存
-> JudgeProtocol 编码
-> ClientSockets 回包
```

这样你就不会掉进细节泥潭。

---

## 二十、看函数调用链时，最容易误解的 3 件事

### 误解 1：谁最重要，谁就一定最先执行

不一定。

比如 `SubmissionService` 很重要，但它不是程序入口。

程序入口还是 `main()`。

### 误解 2：看到线程就说明系统很复杂，看不懂了

你现在先只理解成：

**“前台收到请求后，把慢工作交给后台。”**

这就够了。

### 误解 3：函数一多就说明逻辑很乱

不一定。

很多时候函数拆开反而更好，因为每个函数负责一小块事情。

---

## 二十一、你读完后应该能回答的 6 个问题

如果你看懂了这份文档，你应该可以回答：

1. 哪个函数最先接触到客户端请求？
2. 哪个函数把 JSON 变成内部对象？
3. 哪个函数是真正的评测总调度入口？
4. 哪个函数负责选择语言执行器？
5. 哪个函数负责算出最终 verdict？
6. 哪个函数最后把结果发回去？

如果这 6 个问题你能回答出来，你已经不再是“只会看零散函数”的状态了。

---

## 二十二、最后一句话

读调用链时，你不是在追每一行。

你是在追：

**“控制权现在交给谁了？”**

只要你一直问自己这句话，代码就会越来越清楚。
