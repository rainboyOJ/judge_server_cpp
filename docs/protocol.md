# 协议说明

## 传输层 framing

当前 TCP 协议非常简单：

1. 前 `4` 字节：消息体长度，`uint32_t`，网络字节序（big-endian）。
2. 后续 N 字节：UTF-8 JSON 字符串。

服务端和客户端的请求、响应都遵循同一个 framing。

## 请求 JSON

当前服务端实际接受的字段：

```json
{
  "uuid": 94001,
  "pid": "1000",
  "lang": 2,
  "code": "a, b = map(int, input().split())\nprint(a + b)\n"
}
```

字段说明：

- `uuid`：客户端提交编号。服务端会接收它，但执行阶段实际改用服务端生成的 `submission_id` 作为工作目录键，避免不同客户端重复 `uuid` 时相互覆盖。
- `pid`：题目 ID，对应 `testData/<pid>/data/`。
- `lang`：语言枚举。
  - `0`：C++
  - `1`：C
  - `2`：Python
- `code`：源码字符串。

注意：

- 当前协议枚举里保留了 `C=1`，但服务端实现还不支持，提交会返回 `FAILED + SYSTEM_ERROR`。
- 缺字段、类型错误、非法语言值都会被视为 bad request。

## 成功响应 JSON

```json
{
  "submission_id": 7,
  "status": "FINISHED",
  "verdict": "AC",
  "message": "accepted",
  "code": 0,
  "msg": "accepted",
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

字段说明：

- `submission_id`：服务端创建的真实提交号。
- `status`：当前提交状态。
- `verdict`：最终或当前汇总结论。
- `message`：主消息字段。
- `code` / `msg`：为兼容旧调用方保留的别名字段；正常结果 `code=0`，协议错误包 `code=-1`。
- `case_results`：每个测试点的结果快照数组。

## 错误响应 JSON

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

## 状态值

- `QUEUED`
- `PREPARING`
- `COMPILING`
- `RUNNING`
- `FINISHED`
- `FAILED`

当前 TCP 一次请求只返回一次最终响应，所以客户端通常只会直接看到 `FINISHED` 或 `FAILED`。中间状态主要存在于服务内部和单元测试里。

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

`JudgeCore` 当前采用固定优先级归并：

`CE > SYSTEM_ERROR > TLE > RE/MLE/OLE > WA/PE > UNKNOWN > AC > PENDING`

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

这部分限制需要在生产部署文档和对外说明里明确告知，不应被误认为“完整 OJ 沙箱”。
