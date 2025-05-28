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

module.exports = DataCodec;
