/**
 * TCP连接封装类
 */
const net = require('net');
const { CONFIG } = require('./constants');

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

module.exports = JudgeConnector;
