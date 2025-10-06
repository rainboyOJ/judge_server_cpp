# TCP API 接口文档

## 概述

本文档描述了如何通过TCP协议与评测系统进行交互。系统提供了一个基于TCP的接口，允许客户端提交代码进行评测，并获取评测结果。

## 连接信息

- 默认端口: 8000
- 协议: TCP
- 地址: 127.0.0.1 (本地测试)

## 消息格式

所有消息都采用以下格式：

```
[4字节消息长度(网络字节序)][消息体]
```

### 客户端发送消息格式 (JSON)

```json
{
  "uuid": 12345,           // 唯一评测ID
  "pid": "1000",           // 题目ID
  "lang": 0,               // 编程语言 (0: C++, 1: C, 2: Python)
  "code": "..."            // 源代码
}
```

### 服务端响应消息格式 (JSON)

```json
{
  "code": 0,               // 状态码 (0: 成功, -1: 错误)
  "msg": "...",            // 消息描述
  "data": {                // 评测结果数据 (仅在成功时存在)
    // 评测结果详情
  }
}
```

## 使用示例

### Python 客户端示例

```python
import socket
import json
import struct

def send_test_request():
    # 创建socket连接
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('127.0.0.1', 8000))
    
    # 构造测试数据
    test_data = {
        "uuid": 12345,
        "pid": "1000",
        "lang": 0,  # C++
        "code": "#include <iostream>\nint main() {\n    std::cout << \"Hello, World!\" << std::endl;\n    return 0;\n}"
    }
    
    # 序列化为JSON
    json_str = json.dumps(test_data)
    
    # 发送消息长度
    msg_len = struct.pack('!I', len(json_str))
    sock.send(msg_len)
    
    # 发送JSON数据
    sock.send(json_str.encode('utf-8'))
    
    # 接收响应
    response_len_data = sock.recv(4)
    response_len = struct.unpack('!I', response_len_data)[0]
    
    response_data = sock.recv(response_len)
    response = json.loads(response_data.decode('utf-8'))
    
    print("Response:", response)
    
    sock.close()

if __name__ == "__main__":
    send_test_request()
```

### C++ 客户端示例

参考 `test/tcp_client_test.cpp` 文件。

## 错误处理

服务端可能返回以下错误码：

- `code: -1`: 通用错误
- `code: 0`: 成功

具体的错误信息会在 `msg` 字段中提供。

## 注意事项

1. 所有数据传输都必须按照指定的消息格式进行
2. 消息长度字段必须使用网络字节序(大端序)
3. JSON数据必须是UTF-8编码
4. 客户端应该处理连接异常和超时情况