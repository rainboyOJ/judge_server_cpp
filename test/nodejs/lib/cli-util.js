/**
 * 命令行工具函数
 */
const fs = require('fs');
const { CONFIG, Language } = require('./constants');

/**
 * 解析命令行参数
 */
function parseArguments(args = process.argv.slice(2)) {
    const options = {
        host: CONFIG.host,
        port: CONFIG.port,
        problemId: '1000',
        language: Language.CPP,
        code: `#include <iostream>
using namespace std;

int main() {
    int a, b;
    cin >> a >> b;
    cout << a + b << endl;
    return 0;
}`,
        uuid: Math.floor(Math.random() * 1000000)
    };

    for (let i = 0; i < args.length; i++) {
        switch (args[i]) {
            case '--help':
            case '-h':
                showHelp();
                process.exit(0);
                break;
            case '--host':
                options.host = args[++i];
                break;
            case '--port':
                options.port = parseInt(args[++i]);
                break;
            case '--problem':
            case '--pid':
                options.problemId = args[++i];
                break;
            case '--lang':
                const langMap = { 'cpp': Language.CPP, 'c': Language.C, 'python': Language.PYTHON };
                options.language = langMap[args[++i]] || Language.CPP;
                break;
            case '--code':
                options.code = args[++i];
                break;
            case '--file':
                // 从文件读取代码
                const filePath = args[++i];
                if (fs.existsSync(filePath)) {
                    options.code = fs.readFileSync(filePath, 'utf8');
                } else {
                    console.error(`文件不存在: ${filePath}`);
                    process.exit(1);
                }
                break;
            case '--uuid':
                options.uuid = parseInt(args[++i]);
                break;
            default:
                console.error(`未知参数: ${args[i]}`);
                showHelp();
                process.exit(1);
        }
    }

    return options;
}

/**
 * 显示帮助信息
 */
function showHelp() {
    console.log(`
RainboyOJ 评测客户端 - Node.js版本

使用方法: node send_one_judge.js [选项]

选项:
  --help, -h           显示帮助信息
  --host <host>        服务器地址 (默认: ${CONFIG.host})
  --port <port>        服务器端口 (默认: ${CONFIG.port})
  --problem <id>       题目ID (默认: 1000)
  --lang <language>    编程语言 [cpp|c|python] (默认: cpp)
  --code <code>        源代码字符串
  --file <path>        从文件读取源代码
  --uuid <id>          评测UUID (默认: 随机生成)

示例:
  # 基本使用
  node send_one_judge.js

  # 指定服务器和端口
  node send_one_judge.js --host 192.168.1.100 --port 9090

  # 指定题目和语言
  node send_one_judge.js --problem 1001 --lang python

  # 从文件读取代码
  node send_one_judge.js --file ./solution.cpp --lang cpp

  # 指定代码内容
  node send_one_judge.js --code "print('Hello World')" --lang python
    `);
}

module.exports = {
    parseArguments,
    showHelp
};
