/**
 * 评测客户端主类
 */
const DataCodec = require('./data-codec');
const JudgeConnector = require('./judge-connector');
const { CONFIG, TestError } = require('./constants');

class JudgeClient {
    constructor(host = CONFIG.host, port = CONFIG.port) {
        this.connector = new JudgeConnector(host, port);
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

    /**
     * 获取测试结果摘要信息
     */
    getSummary(result) {
        if (result.errorType === TestError.SUCC) {
            const successCount = result.testPoints.filter(p => p.result === 0).length;
            return `✅ ${successCount}/${result.testPoints.length} 个测试点通过`;
        } else {
            return `❌ 评测失败: ${this.getErrorTypeName(result.errorType)}`;
        }
    }
}

module.exports = JudgeClient;
