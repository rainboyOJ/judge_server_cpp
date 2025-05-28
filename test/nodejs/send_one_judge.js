const net = require('net');
const fs = require('fs');

/**
 * RainboyOJ 评测客户端 - Node.js版本
 * 基于C++代码实现的测试数据编码解码和通信
 */

// 配置常量
const CONFIG = {
    host: process.env.JUDGE_HOST || 'localhost',
    port: parseInt(process.env.JUDGE_PORT) || 8000,
    timeout: 30000,
};

// 语言枚举 (对应C++的language enum)
const Language = {
    CPP: 0,
    C: 1,
    PYTHON: 2
};

// 测试错误类型 (对应C++的testError enum)
const TestError = {
    SUCC: 0,
    PROB_NOT_FOUND: 1,
    DATA_NOT_FOUND: 2,
    COMPILE_ERROR: 3,
    OTHER: 4
};

/**
 * 数据编码解码工具类
 * 基于C++的序列化实现
 */
class DataCodec {
    /**
     * 序列化32位整数 (使用网络字节序)
     */
    static serializeInt32(value) {
        const buffer = Buffer.allocUnsafe(4);
        buffer.writeInt32BE(value, 0);
        return buffer;
    }

    /**
     * 序列化64位整数 (使用网络字节序)
     */
    static serializeInt64(value) {
        const buffer = Buffer.allocUnsafe(8);
        buffer.writeBigInt64BE(BigInt(value), 0);
        return buffer;
    }

    /**
     * 反序列化32位整数
     */
    static deserializeInt32(buffer, offset = 0) {
        return buffer.readInt32BE(offset);
    }

    /**
     * 反序列化64位整数
     */
    static deserializeInt64(buffer, offset = 0) {
        return Number(buffer.readBigInt64BE(offset));
    }

    /**
     * 序列化testProblem结构
     * 对应C++的serializeTestProblem函数
     */
    static serializeTestProblem(testProblem) {
        const buffers = [];
        
        // 1. uuid (4字节)
        buffers.push(this.serializeInt32(testProblem.uuid));
        
        // 2. pid (8字节固定长度)
        const pidBuffer = Buffer.alloc(8);
        const pidStr = testProblem.pid.toString();
        pidBuffer.write(pidStr, 0, Math.min(pidStr.length, 7), 'utf8');
        buffers.push(pidBuffer);
        
        // 3. language (4字节)
        buffers.push(this.serializeInt32(testProblem.language));
        
        // 4. code长度 (4字节) + code内容
        const codeBuffer = Buffer.from(testProblem.code, 'utf8');
        buffers.push(this.serializeInt32(codeBuffer.length));
        buffers.push(codeBuffer);
        
        return Buffer.concat(buffers);
    }

    /**
     * 反序列化testProblem结构
     */
    static deserializeTestProblem(buffer) {
        let offset = 0;
        
        // 1. uuid
        const uuid = this.deserializeInt32(buffer, offset);
        offset += 4;
        
        // 2. pid (8字节)
        const pidBuffer = buffer.subarray(offset, offset + 8);
        const pid = pidBuffer.toString('utf8').replace(/\0/g, '');
        offset += 8;
        
        // 3. language
        const language = this.deserializeInt32(buffer, offset);
        offset += 4;
        
        // 4. code长度和内容
        const codeLength = this.deserializeInt32(buffer, offset);
        offset += 4;
        const code = buffer.subarray(offset, offset + codeLength).toString('utf8');
        
        return { uuid, pid, language, code };
    }

    /**
     * 反序列化testResult结构
     * 对应C++的deserializeTestPointResult函数
     */
    static deserializeTestResult(buffer) {
        let offset = 0;
        
        // 1. uuid
        const uuid = this.deserializeInt32(buffer, offset);
        offset += 4;
        
        // 2. error_type
        const errorType = this.deserializeInt32(buffer, offset);
        offset += 4;
        
        // 3. 测试点结果数组长度
        const resultCount = this.deserializeInt32(buffer, offset);
        offset += 4;
        
        // 4. 解析每个测试点结果
        const testPoints = [];
        for (let i = 0; i < resultCount; i++) {
            const point = {
                seqId: this.deserializeInt32(buffer, offset),
                cpuTime: this.deserializeInt32(buffer, offset + 4),
                realTime: this.deserializeInt32(buffer, offset + 8),
                memory: this.deserializeInt64(buffer, offset + 12),
                signal: this.deserializeInt32(buffer, offset + 20),
                exitCode: this.deserializeInt32(buffer, offset + 24),
                error: this.deserializeInt32(buffer, offset + 28),
                result: this.deserializeInt32(buffer, offset + 32)
            };
            testPoints.push(point);
            offset += 36; // 每个测试点结果36字节
        }
        
        return {
            uuid,
            errorType,
            testPoints
        };
    }
}

/**
 * TCP连接封装类
 */
class JudgeConnector {
    constructor(host, port) {
        this.host = host;
        this.port = port;
        this.socket = null;
    }

    /**
     * 连接到评测服务器
     */
    async connect() {
        return new Promise((resolve, reject) => {
            this.socket = new net.Socket();
            this.socket.setTimeout(CONFIG.timeout);
            
            this.socket.on('connect', () => {
                console.log(`✅ 成功连接到 ${this.host}:${this.port}`);
                resolve();
            });
            
            this.socket.on('error', (err) => {
                console.error('❌ 连接错误:', err.message);
                reject(err);
            });
            
            this.socket.on('timeout', () => {
                console.error('⏰ 连接超时');
                this.socket.destroy();
                reject(new Error('Connection timeout'));
            });
            
            this.socket.connect(this.port, this.host);
        });
    }

    /**
     * 发送数据 (先发送长度，再发送内容)
     */
    async sendData(data) {
        return new Promise((resolve, reject) => {
            if (!this.socket) {
                reject(new Error('Socket not connected'));
                return;
            }

            // 先发送数据长度 (4字节，网络字节序)
            const lengthBuffer = Buffer.allocUnsafe(4);
            lengthBuffer.writeInt32BE(data.length, 0);
            
            console.log(`📤 发送数据长度: ${data.length} 字节`);
            this.socket.write(lengthBuffer);
            
            // 再发送实际数据
            console.log('📤 发送数据内容:');
            this.debugPrintBuffer(data);
            
            this.socket.write(data, (err) => {
                if (err) {
                    reject(err);
                } else {
                    console.log('✅ 数据发送完成');
                    resolve();
                }
            });
        });
    }

    /**
     * 接收数据 (先接收长度，再接收内容)
     */
    async receiveData() {
        return new Promise((resolve, reject) => {
            if (!this.socket) {
                reject(new Error('Socket not connected'));
                return;
            }

            let lengthReceived = false;
            let expectedLength = 0;
            let receivedData = Buffer.alloc(0);
            
            this.socket.on('data', (chunk) => {
                if (!lengthReceived) {
                    // 接收数据长度
                    if (chunk.length >= 4) {
                        expectedLength = chunk.readInt32BE(0);
                        console.log(`📥 接收到数据长度: ${expectedLength} 字节`);
                        lengthReceived = true;
                        
                        // 如果这个chunk还包含数据内容
                        if (chunk.length > 4) {
                            receivedData = Buffer.concat([receivedData, chunk.subarray(4)]);
                        }
                    }
                } else {
                    // 接收数据内容
                    receivedData = Buffer.concat([receivedData, chunk]);
                }
                
                // 检查是否接收完整
                if (lengthReceived && receivedData.length >= expectedLength) {
                    console.log('📥 数据接收完成:');
                    this.debugPrintBuffer(receivedData.subarray(0, expectedLength));
                    resolve(receivedData.subarray(0, expectedLength));
                }
            });
            
            this.socket.on('error', reject);
            this.socket.on('close', () => {
                if (!lengthReceived || receivedData.length < expectedLength) {
                    reject(new Error('Connection closed before receiving complete data'));
                }
            });
        });
    }

    /**
     * 关闭连接
     */
    close() {
        if (this.socket) {
            this.socket.destroy();
            this.socket = null;
            console.log('🔌 连接已关闭');
        }
    }

    /**
     * 调试输出Buffer内容
     */
    debugPrintBuffer(buffer) {
        console.log('-------------------');
        console.log('Hex:', buffer.toString('hex'));
        console.log('Length:', buffer.length, 'bytes');
        
        // 尝试显示可打印字符
        let printable = '';
        for (let i = 0; i < Math.min(buffer.length, 100); i++) {
            const byte = buffer[i];
            printable += (byte >= 32 && byte <= 126) ? String.fromCharCode(byte) : '.';
        }
        console.log('Printable:', printable);
        console.log('-------------------');
    }
}

/**
 * 评测客户端主类
 */
class JudgeClient {
    constructor() {
        this.connector = new JudgeConnector(CONFIG.host, CONFIG.port);
    }

    /**
     * 发送评测请求
     */
    async sendJudgeRequest(testProblem) {
        try {
            console.log('🚀 开始评测请求...\n');
            
            // 连接服务器
            await this.connector.connect();
            
            // 编码测试数据
            console.log('📝 测试数据:', testProblem);
            const encodedData = DataCodec.serializeTestProblem(testProblem);
            
            // 发送数据
            await this.connector.sendData(encodedData);
            
            // 接收结果
            console.log('\n⏳ 等待评测结果...');
            const resultData = await this.connector.receiveData();
            
            // 解码结果
            const result = DataCodec.deserializeTestResult(resultData);
            
            // 输出结果
            this.printResult(result);
            
            return result;
            
        } catch (error) {
            console.error('❌ 评测失败:', error.message);
            throw error;
        } finally {
            this.connector.close();
        }
    }

    /**
     * 格式化输出评测结果
     */
    printResult(result) {
        console.log('\n📊 ===== 评测结果 =====');
        console.log(`UUID: ${result.uuid}`);
        console.log(`错误类型: ${this.getErrorTypeName(result.errorType)}`);
        console.log(`测试点数量: ${result.testPoints.length}`);
        
        if (result.testPoints.length > 0) {
            console.log('\n📋 测试点详情:');
            console.log('序号\tCPU时间\t实际时间\t内存\t\t信号\t退出码\t错误\t结果');
            console.log('----\t-------\t--------\t--------\t----\t------\t----\t----');
            
            result.testPoints.forEach(point => {
                console.log(
                    `${point.seqId}\t${point.cpuTime}ms\t${point.realTime}ms\t\t${point.memory}KB\t\t${point.signal}\t${point.exitCode}\t${point.error}\t${point.result}`
                );
            });
        }
        console.log('========================\n');
    }

    /**
     * 获取错误类型名称
     */
    getErrorTypeName(errorType) {
        const names = {
            [TestError.SUCC]: '成功',
            [TestError.PROB_NOT_FOUND]: '题目不存在',
            [TestError.DATA_NOT_FOUND]: '数据不存在',
            [TestError.COMPILE_ERROR]: '编译错误',
            [TestError.OTHER]: '其他错误'
        };
        return names[errorType] || `未知错误(${errorType})`;
    }
}

/**
 * 命令行参数解析
 */
function parseArguments() {
    const args = process.argv.slice(2);
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

/**
 * 主函数
 */
async function main() {
    try {
        // 解析命令行参数
        const options = parseArguments();
        
        // 更新连接配置
        CONFIG.host = options.host;
        CONFIG.port = options.port;
        
        // 构造测试数据
        const testProblem = {
            uuid: options.uuid,
            pid: options.problemId,
            language: options.language,
            code: options.code
        };
        
        // 创建客户端并发送请求
        const client = new JudgeClient();
        const result = await client.sendJudgeRequest(testProblem);
        
        // 输出摘要信息
        console.log('🎉 评测完成!');
        if (result.errorType === TestError.SUCC) {
            const successCount = result.testPoints.filter(p => p.result === 0).length;
            console.log(`✅ ${successCount}/${result.testPoints.length} 个测试点通过`);
        } else {
            console.log(`❌ 评测失败: ${client.getErrorTypeName(result.errorType)}`);
        }
        
    } catch (error) {
        console.error('💥 程序异常:', error.message);
        process.exit(1);
    }
}

// 处理优雅退出
process.on('SIGINT', () => {
    console.log('\n👋 程序被用户中断');
    process.exit(0);
});

process.on('SIGTERM', () => {
    console.log('\n👋 程序终止');
    process.exit(0);
});

// 运行主程序
if (require.main === module) {
    main();
}

// 导出模块供其他文件使用
module.exports = {
    DataCodec,
    JudgeConnector,
    JudgeClient,
    Language,
    TestError,
    CONFIG
};
