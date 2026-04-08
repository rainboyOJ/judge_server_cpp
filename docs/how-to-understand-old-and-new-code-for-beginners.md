# 给代码小白的旧代码和新代码关系说明

这是很多人第一次读这个项目时最容易疑惑的地方：

**为什么这个项目里看起来像有两套东西？**

比如你会看到：

- 一边有 `SubmissionService`、`JudgeProtocol`、`CppRunner`
- 一边又还有 `testBox`、`resultContainer`、旧的 `judgeInfo`

于是你会怀疑：

“到底哪个才是真的？”

答案是：

**两个都是真的，但它们属于不同阶段。**

这个项目现在处在一个“旧系统往新系统迁移”的过渡期。

所以这份文档就是专门帮你看懂：

- 什么是旧代码
- 什么是新代码
- 它们为什么同时存在
- 你读代码时应该优先看哪一边

---

## 一、先用一句话说明

你可以把现在的项目理解成：

**“旧 TCP 外壳 + 新判题内核”**

翻译成人话就是：

- 外面接请求、管连接的一部分旧东西还在
- 里面真正判题的主流程，已经开始走新设计了

---

## 二、什么叫“旧代码”

旧代码主要指这些：

- `testBox`
- `resultContainer`
- `judgeInfo` 相关结构
- 一部分 `client_sockets` 里的老兼容逻辑
- 一部分 `test_process` 里的历史流程

### 这些旧代码以前是干嘛的

简单说：

旧系统原来已经能做评测，只是结构没有现在这么清晰。

它更多像是：

- 若干模块慢慢长出来
- 功能能跑
- 但职责边界没那么干净

所以你看到旧代码时，常常会觉得：

- 名字有点历史感
- 数据结构比较老
- 一些模块耦合得比较紧

这是正常的。

---

## 三、什么叫“新代码”

新代码主要是这几层：

- `protocol`
- `service`
- `runner`
- `judge`
- `store`

也就是这些核心模块：

- `JudgeProtocol`
- `SubmissionService`
- `RunnerFactory`
- `CppRunner`
- `PythonRunner`
- `JudgeCore`
- `ResultStore`

### 新代码的目标是什么

新代码不是为了“把旧代码换个名字”。

新代码的目标是：

- 结构更清楚
- 职责更单一
- 更容易扩语言
- 更容易测
- 更容易维护

所以你会看到新代码更喜欢这样的组织方式：

- 一层只做一件事
- 通过接口协作
- 测试更明确

---

## 四、为什么它们会同时存在

这才是关键。

答案很简单：

**因为系统不能一夜之间完全重写。**

如果一下子把旧代码全删掉，再把新系统一次性替换进去，风险会很大：

- 容易把原本还能用的功能弄坏
- 容易一下改太多，测试跟不上
- 容易根本不知道 bug 是新问题还是迁移问题

所以更稳妥的做法是：

### 先保留旧外壳

比如：

- `main.cpp` 还会创建 `testBox`
- `ClientSockets` 还带有一部分旧管理方式

### 再慢慢把核心流程换成新链路

比如：

- 新请求现在优先走 `JudgeProtocol -> SubmissionService`
- 真正的 `C++/Python` 判题主线已经切到新的 runner 体系

所以你看到“新旧并存”并不代表代码混乱到不能读。

它更像是在“搬家过程中”。

---

## 五、你可以怎么区分旧代码和新代码

一个很简单的判断方法：

### 如果它更像“统一模型 + 清晰分层”

那通常是新代码。

比如：

- `SubmissionRequest`
- `SubmissionResult`
- `JudgeProtocol`
- `SubmissionService`
- `ResultStore`

### 如果它更像“历史流程里的管理对象”

那通常是旧代码。

比如：

- `testBox`
- `resultContainer`
- `testSession`
- 旧 `judgeInfo` 结构

---

## 六、你读代码时，应该优先看哪一边

对于你这种“现在准备人工阅读代码，而且还是代码小白”的情况，最重要的建议是：

### 先看新代码主线

也就是先看：

- `JudgeProtocol`
- `SubmissionService`
- `RunnerFactory`
- `CppRunner`
- `PythonRunner`
- `JudgeCore`
- `ResultStore`

因为这一条线更清楚，更适合你建立整体认知。

### 旧代码先只把它当“兼容外壳”理解

也就是说，你现在对旧代码不需要一开始就搞懂所有细节。

你只需要先知道：

- 它还存在
- 它还承担部分连接管理和历史流程职责
- 但新判题主线已经不完全依赖它了

---

## 七、最容易让新手混乱的地方

## 1. `ClientSockets` 为什么看起来又新又旧

因为它确实是过渡层。

它现在做两件事：

- 处理新 JSON 请求
- 保留旧 `testBox` 回写 fallback

所以你会在同一个文件里同时看到：

- `JudgeProtocol`
- `SubmissionService`
- `testBox`

这不是你看错了。

它就是一个桥。

---

## 2. `test_process` 为什么还在

因为新 runner 并不是完全从零复制一套能力。

而是从旧流程里抽取可复用的部分，比如：

- 编译 helper
- 单点执行 helper
- 输出比较 helper

所以 `test_process` 不是“完全废弃代码”。

它的一部分已经变成了新 runner 的底层工具。

---

## 3. 为什么 `main.cpp` 还在创建 `testBox`

因为整个系统还没有彻底去掉旧连接容量管理和一部分历史回调关系。

也就是说：

- 新判题内核已经换了很多
- 但最外层外壳还没有完全换掉

这就是典型的渐进迁移。

---

## 八、你读代码时的正确心态

面对这种新旧并存的项目，你最好的阅读策略不是：

“我要先把旧系统全搞懂，再看新系统。”

这样会很累。

更好的方法是：

### 第一步：先抓新主线

理解：

- 请求怎么解析
- 提交怎么调度
- runner 怎么执行
- verdict 怎么汇总

### 第二步：再回头看旧代码在外层的作用

理解：

- 为什么还保留 `testBox`
- 为什么 `client_sockets` 还有 fallback
- 为什么 `test_process` 还没彻底消失

这样你就不会一开始被历史包袱压住。

---

## 九、你可以把它想成一栋正在装修的房子

这个比喻通常很好懂。

### 旧代码

像房子的旧结构：

- 还能住
- 还能用
- 但不够理想

### 新代码

像新装修的房间：

- 更整洁
- 功能划分更清楚
- 以后更容易继续扩展

### 为什么不把旧房子一下子全拆了

因为人还在里面生活。

同理，系统还在演进，不能一下全推倒重来。

---

## 十、如果你现在只想知道“我到底该盯哪条线读”

那我直接给你最短答案：

### 主要阅读线

- `main.cpp`
- `src/client_sockets.cpp`
- `src/protocol/JudgeProtocol.cpp`
- `src/service/SubmissionService.cpp`
- `src/runner/RunnerFactory.cpp`
- `src/runner/CppRunner.cpp`
- `src/runner/PythonRunner.cpp`
- `src/judge/JudgeCore.cpp`
- `src/store/ResultStore.cpp`

### 作为补充理解的旧代码

- `src/testBox.cpp`
- `src/resultContainer.cpp`
- `src/test_process/*.cpp`

所以一句话总结就是：

**先读新内核，后补旧外壳。**

---

## 十一、你现在应该能回答的几个问题

如果你看完这份文档，你应该能回答：

1. 为什么项目里会同时有 `SubmissionService` 和 `testBox`？
2. 为什么 `client_sockets.cpp` 里会同时出现新旧逻辑？
3. 为什么 `test_process` 还没彻底消失？
4. 现在真正应该优先理解的是哪条主线？

如果这 4 个问题你能答出来，你以后读这个项目就不会总觉得“怎么有两套系统”。

---

## 十二、最后一句最重要的话

这个项目不是“完全旧系统”，也不是“完全新系统”。

它现在是：

**旧外壳还在，新内核已经长出来。**

你只要先抓住新内核，再回头理解旧外壳，阅读难度就会下降很多。
