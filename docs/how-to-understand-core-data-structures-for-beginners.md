# 给代码小白的核心数据结构解释版

如果说上一份文档是在讲：

**“这个项目里的文件应该怎么读”**

那么这一份讲的是：

**“这个项目里最重要的几个数据结构，到底是在装什么东西”**

很多代码小白读项目时，真正卡住的不是函数本身，反而是：

- 这个结构体为什么存在？
- 这里面这些字段是什么意思？
- 为什么这个函数要传这个对象？

所以这份文档专门解决这个问题。

---

## 一、先记住一个最重要的阅读原则

你看到一个数据结构时，不要先问：

**“这个字段是什么语法？”**

你要先问：

1. 这个结构体像现实中的什么东西？
2. 它在项目流程里哪一步出现？
3. 它是给谁用的？

只要这 3 个问题清楚了，读数据结构就会轻松很多。

---

## 二、这个项目里最重要的数据结构有哪些

你现在最值得先理解的是这几类：

1. `SubmissionRequest`
2. `SubmissionStatus`
3. `SubmissionVerdict`
4. `SubmissionCaseResult`
5. `SubmissionResult`
6. `RunnerPrepareResult`
7. `RunnerCompileResult`
8. `RunnerCaseInput`
9. `RunnerCaseResult`

它们主要在这些文件里定义：

- `include/common/SubmissionTypes.h`
- `include/runner/ILanguageRunner.h`

---

## 三、逐个解释

## 1. `SubmissionRequest`

定义位置：

- `include/common/SubmissionTypes.h`

代码里长这样：

- `uuid`
- `pid`
- `language`
- `code`

### 它像什么？

它像一张“用户提交单”。

用户每提交一次代码，系统就会先把这次提交整理成一个 `SubmissionRequest`。

### 每个字段是什么意思？

#### `uuid`

这是客户端带来的一个标识。

你可以先把它理解成：

**“这份提交原来是谁的、来自哪里”**

注意：在执行阶段，服务层会把它换成服务端自己的 `submission_id` 来做工作目录隔离。

#### `pid`

题目编号。

比如：

- `1000`

系统会根据它去找：

- `testData/<pid>/data`

也就是去找这个题目的测试点。

#### `language`

提交语言。

当前主要是：

- `CPP = 0`
- `C = 1`
- `PYTHON = 2`

注意：

虽然 `C` 这个值保留着，但当前真正支持的是：

- `C++`
- `Python`

#### `code`

用户提交的源代码。

也就是要被编译或执行的那段文本。

### 它在什么时候出现？

它最早出现在协议层：

- `JudgeProtocol::decodeRequest()`

也就是：

**网络 JSON 请求 -> `SubmissionRequest`**

### 你应该记住什么？

`SubmissionRequest` 就是：

**“一份待评测的代码提交”**

---

## 2. `SubmissionStatus`

定义位置：

- `include/common/SubmissionTypes.h`

它的值有：

- `QUEUED`
- `PREPARING`
- `COMPILING`
- `RUNNING`
- `FINISHED`
- `FAILED`

### 它像什么？

它像“当前进度条状态”。

### 每个状态是什么意思？

#### `QUEUED`

刚收到提交，还没开始真正处理。

#### `PREPARING`

正在做准备工作。

比如：

- 创建工作目录
- 写代码文件

#### `COMPILING`

正在编译，或者做语法检查。

注意：

Python 虽然不是传统编译型语言，但这里也会走“compile-check”阶段。

#### `RUNNING`

正在逐个测试点执行。

#### `FINISHED`

流程正常结束了。

注意：正常结束不代表一定是 `AC`。

也可能是：

- `CE`
- `WA`
- `TLE`

#### `FAILED`

流程在系统层面失败了。

比如：

- 找不到 runner
- 找不到测试数据
- 内部异常

### 你应该记住什么？

`SubmissionStatus` 是在回答：

**“这次提交现在做到哪一步了？”**

---

## 3. `SubmissionVerdict`

定义位置：

- `include/common/SubmissionTypes.h`

它的值有：

- `PENDING`
- `AC`
- `WA`
- `TLE`
- `MLE`
- `RE`
- `OLE`
- `PE`
- `CE`
- `UNKNOWN`
- `SYSTEM_ERROR`

### 它像什么？

它像“最后判决书”。

### 最常见几个值你先记什么

#### `AC`

答案通过。

#### `WA`

答案错了。

#### `CE`

编译失败。

#### `RE`

运行时出错。

#### `TLE`

超时。

#### `SYSTEM_ERROR`

不是用户代码错，而是系统内部出问题。

### `Status` 和 `Verdict` 有什么区别？

这是很多新手最容易混的地方。

记住：

- `Status` = 进度走到哪一步
- `Verdict` = 最后判成什么结果

比如：

- `status = RUNNING`
- `verdict = PENDING`

意思就是：

**“还在跑，所以现在还没最终判出来。”**

又比如：

- `status = FINISHED`
- `verdict = CE`

意思就是：

**“流程结束了，最终结果是编译错误。”**

---

## 4. `SubmissionCaseResult`

定义位置：

- `include/common/SubmissionTypes.h`

它里面有：

- `seq_id`
- `verdict`
- `cpu_time_ms`
- `real_time_ms`
- `memory_kb`
- `signal`
- `exit_code`
- `error_code`

### 它像什么？

它像“某一个测试点的批改记录”。

注意：

不是整道题的结果。

只是一个测试点的结果。

### 你第一次重点看什么

#### `seq_id`

第几个测试点。

#### `verdict`

这个测试点是：

- 过了
- 错了
- 超时了
- 运行错了

#### `cpu_time_ms` / `real_time_ms`

用了多少时间。

#### `memory_kb`

用了多少内存。

#### `exit_code`

程序退出码。

如果不是 `0`，通常说明程序运行不正常。

### 你应该记住什么

`SubmissionCaseResult` 是：

**“一个测试点的成绩单”。**

---

## 5. `SubmissionResult`

定义位置：

- `include/common/SubmissionTypes.h`

它里面有：

- `submission_id`
- `status`
- `verdict`
- `message`
- `case_results`

### 它像什么？

它像“整次提交的总成绩单”。

### 每个字段怎么理解

#### `submission_id`

这是服务端自己生成的内部编号。

这个比客户端给的 `uuid` 更可靠，因为它是服务端自己管的。

#### `status`

当前进度。

#### `verdict`

当前最终结论。

在还没跑完时通常是：

- `PENDING`

#### `message`

补充信息。

比如：

- 编译错误信息
- 运行错误信息
- 失败原因

#### `case_results`

所有测试点结果列表。

也就是很多个 `SubmissionCaseResult` 放在一起。

### 它在流程里是什么角色？

你可以把它理解成：

**整个提交在某个时刻的“完整快照”。**

这也是为什么 `ResultStore` 里存的是它。

---

## 6. `RunnerPrepareResult`

定义位置：

- `include/runner/ILanguageRunner.h`

字段很简单：

- `ok`
- `message`

### 它像什么？

它像“准备工作有没有做好”的回执。

### 什么时候会用到

比如：

- 工作目录创建失败
- 源码文件写失败

### 你应该记住什么

这是 runner 在 `prepare()` 阶段返回的小结果。

---

## 7. `RunnerCompileResult`

定义位置：

- `include/runner/ILanguageRunner.h`

字段有：

- `ok`
- `verdict`
- `message`

### 它像什么？

它像“编译阶段的批注”。

### 为什么除了 `ok` 还要有 `verdict`

因为编译失败时，系统不是只想知道“失败了”，还想知道这属于什么判题结果。

例如：

- `CE`

### 你应该记住什么

`RunnerCompileResult` 是：

**“编译/语法检查阶段的小结论”。**

---

## 8. `RunnerCaseInput`

定义位置：

- `include/runner/ILanguageRunner.h`

里面有：

- `seq_id`
- `input_path`
- `expected_output_path`
- `cpu_time_limit_ms`
- `real_time_limit_ms`
- `memory_limit_kb`

### 它像什么？

它像“给某个测试点下发的任务单”。

### 它告诉 runner 什么

就是：

- 这是第几个测试点
- 输入文件在哪
- 标准输出在哪
- 限时多少
- 限内存多少

### 你应该记住什么

runner 跑单点时，不是只拿代码就够了。

它还得知道：

**这个测试点该怎么跑。**

---

## 9. `RunnerCaseResult`

定义位置：

- `include/runner/ILanguageRunner.h`

里面有：

- `result`
- `message`

其中 `result` 本身就是一个 `SubmissionCaseResult`。

### 它像什么？

它像“单点执行后的回执 + 附加说明”。

### 为什么不直接只返回 `SubmissionCaseResult`

因为有时还想顺带带一段文字说明。

比如：

- 输出不匹配
- 启动失败
- 运行错误信息

### 你应该记住什么

它其实就是：

**“测试点结果 + 一句补充说明”**

---

## 四、这些结构之间的关系

你可以把它们想成这样一条链：

### 第 1 步：先收到请求

用：

- `SubmissionRequest`

### 第 2 步：服务层记录当前进度

用：

- `SubmissionStatus`
- `SubmissionResult`

### 第 3 步：runner 开始准备和编译

用：

- `RunnerPrepareResult`
- `RunnerCompileResult`

### 第 4 步：runner 跑每个测试点

输入：

- `RunnerCaseInput`

输出：

- `RunnerCaseResult`

### 第 5 步：把每个点的结果汇总进总结果

用：

- `SubmissionCaseResult`
- `SubmissionResult`
- `SubmissionVerdict`

---

## 五、如果你总是分不清这些结构，记住这个表

### `SubmissionRequest`

用户交上来的“提交单”

### `SubmissionCaseResult`

单个测试点的“成绩单”

### `SubmissionResult`

整次提交的“总成绩单”

### `RunnerCaseInput`

给 runner 的“测试点任务单”

### `RunnerCaseResult`

runner 返回的“测试点执行回执”

---

## 六、你现在最应该重点记住哪 3 个

如果你一下记不住全部，那就先重点记住这 3 个：

1. `SubmissionRequest`
2. `SubmissionResult`
3. `SubmissionCaseResult`

因为它们分别代表：

- 输入
- 总输出
- 局部输出

这 3 个一旦明白了，你看其他结构时会顺很多。

---

## 七、最后送你一个读数据结构的小技巧

以后你每看到一个新的结构体，都试着用这种句式解释它：

**“这个结构体是用来表示 ______ 的，它在 ______ 阶段出现，主要给 ______ 用。”**

比如：

- `SubmissionRequest` 是用来表示“一次用户代码提交”的，它在协议解析之后出现，主要给 `SubmissionService` 和 runner 用。

如果你能自己说出这种话，就说明你不是只在“看代码”，而是真的在理解代码。

---

如果你愿意，我下一步还可以继续给你写第 3 份：

1. `给小白的函数调用链解释版`
2. 或者 `一次提交从头到尾发生了什么（故事版）`
