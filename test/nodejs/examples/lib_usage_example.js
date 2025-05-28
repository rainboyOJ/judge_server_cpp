/**
 * 示例: 如何使用 RainboyOJ 客户端库
 * 
 * 这个示例展示了如何在您的代码中集成和使用 RainboyOJ 评测客户端库
 */

// 导入客户端库模块
const { JudgeClient, Language, TestError } = require('./lib/index.js');

// 设置您的评测服务器信息
const SERVER_HOST = 'localhost';
const SERVER_PORT = 8000;

/**
 * 示例1: 评测一个简单的C++程序
 */
async function evaluateCppCode() {
    console.log('示例1: 评测C++代码');
    
    // 创建评测客户端
    const client = new JudgeClient(SERVER_HOST, SERVER_PORT);
    
    // 准备测试数据
    const testData = {
        uuid: Math.floor(Math.random() * 1000000),
        pid: '1000',  // A+B 问题
        language: Language.CPP,
        code: `
#include <iostream>
using namespace std;

int main() {
    int a, b;
    cin >> a >> b;
    cout << a + b << endl;
    return 0;
}
        `.trim()
    };
    
    try {
        // 发送评测请求
        const result = await client.sendJudgeRequest(testData);
        
        // 处理评测结果
        console.log('评测完成!');
        console.log(client.getSummary(result));
        
        // 您可以进一步处理结果
        if (result.errorType === TestError.SUCC) {
            console.log('所有测试点:', result.testPoints);
        } else {
            console.log('评测失败原因:', client.getErrorTypeName(result.errorType));
        }
    } catch (error) {
        console.error('评测请求失败:', error.message);
    }
}

/**
 * 示例2: 从文件读取代码并评测
 */
async function evaluateFromFile(filePath) {
    console.log(`示例2: 从文件 ${filePath} 读取代码并评测`);
    
    const fs = require('fs');
    
    try {
        // 读取文件内容
        const code = fs.readFileSync(filePath, 'utf8');
        
        // 根据文件扩展名确定语言
        let language = Language.CPP;
        if (filePath.endsWith('.py')) {
            language = Language.PYTHON;
        } else if (filePath.endsWith('.c')) {
            language = Language.C;
        }
        
        // 创建客户端并评测
        const client = new JudgeClient(SERVER_HOST, SERVER_PORT);
        const result = await client.sendJudgeRequest({
            uuid: Math.floor(Math.random() * 1000000),
            pid: 'hello',  // 示例题目ID
            language: language,
            code: code
        });
        
        // 输出结果摘要
        console.log(client.getSummary(result));
    } catch (error) {
        console.error('评测失败:', error.message);
    }
}

// 如果作为主程序运行，则执行示例
if (require.main === module) {
    // 调用示例函数
    evaluateCppCode()
        .then(() => console.log('\n示例1执行完成'))
        .catch(console.error);
    
    // 可以取消注释下面的代码来运行第二个示例
    // const exampleFile = './examples/hello.py';
    // setTimeout(() => {
    //     evaluateFromFile(exampleFile)
    //         .then(() => console.log('\n示例2执行完成'))
    //         .catch(console.error);
    // }, 2000);
}

// 导出示例函数，以便其他文件可以导入和使用
module.exports = {
    evaluateCppCode,
    evaluateFromFile
};
