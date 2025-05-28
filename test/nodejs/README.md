# RainboyOJ Node.js 评测客户端

这是一个用于与 RainboyOJ 评测系统通信的 Node.js 客户端。支持多种编程语言的代码提交、评测结果解析和展示。

## 目录结构

```
nodejs/
  ├── lib/                  # 核心库模块
  │   ├── constants.js      # 常量和枚举定义
  │   ├── data-codec.js     # 数据编解码工具
  │   ├── judge-connector.js # TCP通信封装
  │   ├── judge-client.js   # 评测客户端主类
  │   ├── cli-util.js       # 命令行工具函数
  │   └── index.js          # 模块导出入口
  ├── examples/             # 示例代码
  │   ├── a_plus_b.cpp      # C++ A+B题示例
  │   ├── hello.py          # Python示例
  │   ├── hello.c           # C语言示例
  │   ├── lib_usage_example.js # 库使用示例
  │   └── batch_tester.js   # 批量测试工具
  ├── send_one_judge.js     # 原始客户端实现
  ├── send_one_judge_new.js # 基于模块化库实现的新版客户端
  ├── test_codec.js         # 原始编解码测试
  └── test_codec_new.js     # 基于模块化库的编解码测试
```

## 功能特性

- 完全符合 RainboyOJ 评测系统通信协议
- 支持多种编程语言（C++、C、Python）
- 提供低级数据编解码API
- 提供高级评测客户端接口
- 支持命令行和编程方式使用
- 包含完整的文档和示例
- 提供批量测试工具

## 快速开始

### 命令行使用

```bash
# 基本使用
node send_one_judge_new.js

# 指定服务器和端口
node send_one_judge_new.js --host 192.168.1.100 --port 9090

# 提交C++文件
node send_one_judge_new.js --file examples/a_plus_b.cpp --lang cpp --problem 1000

# 提交Python文件
node send_one_judge_new.js --file examples/hello.py --lang python --problem hello
```

### 在代码中使用

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

## 高级用法

### 批量测试

```bash
# 批量测试一个目录中的所有代码文件
node examples/batch_tester.js ./solutions 1000
```

或在代码中使用批量测试工具:

```javascript
const BatchTester = require('./examples/batch_tester');
const tester = new BatchTester('localhost', 8000);

// 测试目录中的所有文件
tester.testDirectory('./solutions', '1000')
    .then(results => {
        console.log('测试完成，共测试', results.length, '个文件');
    });
```

## 开发文档

请参阅 [lib/README.md](./lib/README.md) 获取详细的API文档和使用说明。

## 示例

- [lib_usage_example.js](./examples/lib_usage_example.js): 演示如何在代码中使用客户端库
- [batch_tester.js](./examples/batch_tester.js): 批量测试工具，可用于测试多个代码文件

## 更新日志

- 2025-05-27: 重构代码，分离为模块化库结构，添加批量测试工具
- 2025-05-20: 初始版本完成，实现基本评测功能
