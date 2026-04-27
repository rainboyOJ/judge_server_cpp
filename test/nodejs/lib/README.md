# RainboyOJ Node.js 客户端库

这是一个用于与 RainboyOJ 评测系统通信的 Node.js 客户端库。它提供了编码解码、网络连接和评测请求等功能。

## 目录结构

```
lib/
  ├── constants.js     # 常量和枚举定义
  ├── data-codec.js    # 数据编解码工具
  ├── judge-connector.js # TCP通信封装
  ├── judge-client.js  # 评测客户端主类
  ├── cli-util.js      # 命令行工具函数
  └── index.js         # 模块导出入口
```

## 使用方法

### 基本使用

```javascript
const { JudgeClient, Language } = require('./lib');

// 创建评测客户端
const client = new JudgeClient('localhost', 8000);

// 构造测试数据
const testProblem = {
    uuid: 12345,
    pid: '1000',
    language: Language.CPP,
    code: `#include <iostream>
using namespace std;

int main() {
    int a, b;
    cin >> a >> b;
    cout << a + b << endl;
    return 0;
}`
};

// 发送评测请求
client.sendJudgeRequest(testProblem)
    .then(result => {
        console.log('评测结果:', result);
        console.log('摘要:', client.getSummary(result));
    })
    .catch(err => {
        console.error('评测失败:', err);
    });
```

### 作为命令行工具

```bash
# 基本使用
node ../send_one_judge.js

# 指定服务器和端口
node ../send_one_judge.js --host 192.168.1.100 --port 9090

# 从文件读取代码
node ../send_one_judge.js --file ./solution.cpp --lang cpp
```

## API 文档

### Constants

- `CONFIG`: 默认配置项
- `Language`: 支持的编程语言枚举
- `TestError`: 测试错误类型枚举

### DataCodec

数据编码解码工具类

- `serializeTestProblem(testProblem)`: 序列化测试题目数据
- `deserializeTestProblem(buffer)`: 反序列化测试题目数据
- `deserializeTestResult(buffer)`: 反序列化测试结果数据

### JudgeConnector

网络连接封装类

- `connect()`: 连接到评测服务器
- `sendData(data)`: 发送数据
- `receiveData()`: 接收数据
- `close()`: 关闭连接

### JudgeClient

评测客户端主类

- `sendJudgeRequest(testProblem)`: 发送评测请求并获取结果
- `printResult(result)`: 格式化输出评测结果
- `getErrorTypeName(errorType)`: 获取错误类型名称
- `getSummary(result)`: 获取测试结果摘要信息

### 工具函数

- `parseArguments(args)`: 解析命令行参数
- `showHelp()`: 显示帮助信息

## 示例

参见 `../demo.js` 和 `../test_codec.js` 文件。
