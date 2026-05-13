# 协议说明

## 传输层 framing

TCP 协议使用统一 framing：

1. 前 4 字节：消息体长度，`uint32_t`，网络字节序（big-endian）。
2. 后续 N 字节：UTF-8 JSON 字符串。

客户端请求和服务端响应都遵循这个 framing。

## 请求类型

### 1. `submit`

```json
{
  "type": "submit",
  "uuid": 94001,
  "pid": "1000",
  "lang": 2,
  "code": "a, b = map(int, input().split())\nprint(a + b)\n"
}
```

字段说明：

- `type`：固定为 `submit`。
- `uuid`：客户端侧提交编号。服务端会接收它，但执行阶段改用服务端生成的 `submission_id` 作为工作目录键。
- `pid`：题目 ID，对应 `testData/<pid>/data/`。
- `lang`：语言枚举。
  - `0`：C++
  - `1`：C
  - `2`：Python
- `code`：源码字符串。

注意：

- `lang=1` 当前仍未实现，会在后续评测阶段写成 `FAILED + SYSTEM_ERROR`。
- 缺字段、类型错误、非法语言值、错误 `type` 都会被视为 bad request。

### 2. `query_result`

```json
{
  "type": "query_result",
  "submission_id": 77
}
```

字段说明：

- `type`：固定为 `query_result`。
- `submission_id`：服务端创建的真实提交号。

该请求只做一次快照查询，不会阻塞等待状态变化。

## 响应类型

### 1. `submission_ack`

`submit` 请求由 `SubmissionService::submitAsync()` 成功创建 submission 并写入内部队列后，服务端立即返回：

```json
{
  "type": "submission_ack",
  "submission_id": 7,
  "status": "QUEUED",
  "verdict": "PENDING",
  "message": "queued",
  "case_results": []
}
```

语义：

- 表示 submission 已在 `ResultStore` 创建，并且已投递到 `SubmissionService` 的内部异步队列。
- 它不表示编译或运行已经开始。

### 2. `submission_update`

用于表达非终态快照；当前最常见来源是 `query_result` 查询到运行中记录：

```json
{
  "type": "submission_update",
  "submission_id": 7,
  "status": "RUNNING",
  "verdict": "PENDING",
  "message": "",
  "case_results": [
    {
      "seq_id": 1,
      "verdict": "AC",
      "cpu_time_ms": 12,
      "real_time_ms": 15,
      "memory_kb": 256,
      "signal": 0,
      "exit_code": 0,
      "error_code": 0
    }
  ]
}
```

语义：

- `status` 可能是 `PREPARING`、`COMPILING`、`RUNNING`。
- `case_results` 表示查询时已经写入 `ResultStore` 的测试点快照。

### 3. `submission_finished`

用于终态快照；既可能是原连接上的 worker 完成推送，也可能是 `query_result` 查询终态结果：

```json
{
  "type": "submission_finished",
  "submission_id": 7,
  "status": "FINISHED",
  "verdict": "AC",
  "message": "accepted",
  "case_results": [
    {
      "seq_id": 1,
      "verdict": "AC",
      "cpu_time_ms": 12,
      "real_time_ms": 15,
      "memory_kb": 256,
      "signal": 0,
      "exit_code": 0,
      "error_code": 0
    }
  ]
}
```

语义：

- `status` 为 `FINISHED` 或 `FAILED`。
- `verdict` 为最终归并结果。
- `case_results` 为截至终态时保留的完整测试点结果数组。

### 4. 错误响应

当前错误包仍保留兼容格式，不带 `type` 字段：

```json
{
  "submission_id": -1,
  "status": "FAILED",
  "verdict": "SYSTEM_ERROR",
  "message": "bad request",
  "code": -1,
  "msg": "bad request",
  "case_results": []
}
```

常见错误场景：

- JSON 结构不合法
- `submit` / `query_result` 请求字段缺失
- `submission_id` 不存在
- 提交创建失败
- `SubmissionService` 内部队列已关闭导致无法入队

## 状态值

- `QUEUED`
- `PREPARING`
- `COMPILING`
- `RUNNING`
- `FINISHED`
- `FAILED`

状态由 `ResultStore` 维护，只允许按如下方向前进：

- `QUEUED → PREPARING | FAILED`
- `PREPARING → COMPILING | FAILED`
- `COMPILING → RUNNING | FINISHED | FAILED`
- `RUNNING → FINISHED | FAILED`

终态后不允许回退或再次更新。

## verdict 值

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

`SubmissionVerdictReducer` 当前采用固定优先级归并：

`CE > SYSTEM_ERROR > TLE > RE/MLE/OLE > WA/PE > UNKNOWN > AC > PENDING`

## 当前实际交互模式

### 提交后原连接仍在线

典型顺序是：

1. client 发送 `submit`
2. server 返回 `submission_ack`
3. worker 后台处理
4. server 在同一连接推送 `submission_finished`

### 提交后原连接断开

典型顺序是：

1. client 发送 `submit`
2. server 返回 `submission_ack`
3. 原连接断开
4. worker 继续处理并把结果写入 `ResultStore`
5. 新连接发送 `query_result`
6. server 返回 `submission_finished`

### 查询运行中任务

如果 `query_result` 命中非终态 submission，响应是 `submission_update`，不是 `submission_finished`。

## 输出比较规则

优先使用 `/judge/checker/fcmp2`：

```bash
/judge/checker/fcmp2 <in> <user_out> <ans>
```

如果 checker 不存在，服务端回退到内置比较：

- 按行读取。
- 忽略每行末尾空格、TAB、`\r`。
- 忽略文件尾部多余空行。

## 资源限制行为

### 有 `/usr/bin/sjudge`

通过 `call_sjudge()` 执行，结果中的 CPU 时间、真实时间、内存、信号、退出码、错误码都来自 sjudge。

### 没有 `/usr/bin/sjudge`

走 fallback：

- 用 `posix_spawn` 启动程序。
- 只按 `real_time_limit_ms` 做超时 kill。
- `cpu_time_ms` 直接记为墙钟时间。
- `memory_kb` 基本不会得到真实值。
- 不提供真正的沙箱隔离。

## 脚本参考

Node 端异步评测演示脚本：`test/nodejs/send_async_judge.js`，运行方式：

```bash
cd test/nodejs && npm run async-judge
```

## 当前限制

- 协议层已经支持 `submission_update`，但当前主动推送路径几乎只会推 `submission_finished`；更细粒度的进度推送还没有接到 notifier 生命周期上。
- `query_result` 只支持按单个 `submission_id` 查询最新快照，没有批量查询或 wait/subscribe 语义。
- 错误包仍使用旧兼容格式，因此成功包和错误包的 JSON 形状暂时不完全统一。
