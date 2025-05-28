/**
 * 批量测试工具
 * 用于批量测试多个题目或代码文件
 */

const fs = require('fs');
const path = require('path');
const { JudgeClient, Language, TestError } = require('../lib/index.js');

class BatchTester {
    constructor(host = 'localhost', port = 8000) {
        this.client = new JudgeClient(host, port);
        this.results = [];
    }

    /**
     * 从目录中读取所有测试代码并提交评测
     * @param {string} directory 代码所在目录
     * @param {string} problemId 要测试的题目ID
     * @param {object} options 选项 { filter: 文件过滤函数, delay: 请求间隔(ms) }
     */
    async testDirectory(directory, problemId, options = {}) {
        const { filter = (file) => file.endsWith('.cpp') || file.endsWith('.py') || file.endsWith('.c'),
               delay = 1000 } = options;

        try {
            // 获取目录中的文件
            const files = fs.readdirSync(directory).filter(filter);
            console.log(`📁 在目录 ${directory} 中找到 ${files.length} 个文件`);

            // 处理每个文件
            for (let i = 0; i < files.length; i++) {
                const file = files[i];
                const fullPath = path.join(directory, file);
                
                console.log(`\n[${i+1}/${files.length}] 测试文件: ${file}`);
                
                // 根据文件扩展名确定语言
                let language = Language.CPP;
                if (file.endsWith('.py')) {
                    language = Language.PYTHON;
                } else if (file.endsWith('.c')) {
                    language = Language.C;
                }
                
                // 读取代码
                const code = fs.readFileSync(fullPath, 'utf8');
                
                // 提交评测
                const result = await this.submitTest({
                    problemId,
                    language,
                    code,
                    fileName: file
                });
                
                this.results.push(result);
                
                // 添加延迟，避免频繁请求
                if (i < files.length - 1) {
                    await new Promise(resolve => setTimeout(resolve, delay));
                }
            }
            
            // 输出汇总结果
            this.printSummary();
            
            return this.results;
            
        } catch (error) {
            console.error('批量测试失败:', error);
            throw error;
        }
    }
    
    /**
     * 提交单个测试
     */
    async submitTest({ problemId, language, code, fileName }) {
        const uuid = Math.floor(Math.random() * 1000000);
        console.log(`📤 提交测试: 题目=${problemId}, 语言=${this.getLanguageName(language)}, UUID=${uuid}`);
        
        try {
            const result = await this.client.sendJudgeRequest({
                uuid,
                pid: problemId,
                language,
                code
            });
            
            // 添加额外信息到结果对象
            result.fileName = fileName;
            result.submittedAt = new Date();
            result.summary = this.client.getSummary(result);
            
            console.log(`📝 测试结果: ${result.summary}`);
            return result;
            
        } catch (error) {
            console.error(`❌ 测试失败: ${error.message}`);
            return {
                uuid,
                errorType: TestError.OTHER,
                testPoints: [],
                fileName,
                submittedAt: new Date(),
                error: error.message,
                summary: `❌ 测试失败: ${error.message}`
            };
        }
    }
    
    /**
     * 输出汇总结果
     */
    printSummary() {
        console.log('\n📊 ===== 测试汇总 =====');
        console.log(`总共测试: ${this.results.length} 个文件`);
        
        const succeeded = this.results.filter(r => r.errorType === TestError.SUCC);
        console.log(`成功评测: ${succeeded.length} 个文件`);
        
        const failed = this.results.filter(r => r.errorType !== TestError.SUCC);
        console.log(`失败评测: ${failed.length} 个文件`);
        
        if (failed.length > 0) {
            console.log('\n❌ 失败的测试:');
            failed.forEach((result, index) => {
                console.log(`${index+1}. ${result.fileName}: ${result.summary || '未知错误'}`);
            });
        }
        
        console.log('====================\n');
    }
    
    /**
     * 获取语言名称
     */
    getLanguageName(languageCode) {
        const names = {
            [Language.CPP]: 'C++',
            [Language.C]: 'C',
            [Language.PYTHON]: 'Python'
        };
        return names[languageCode] || `未知(${languageCode})`;
    }
}

module.exports = BatchTester;

// 如果作为主程序运行，则提供命令行界面
if (require.main === module) {
    const args = process.argv.slice(2);
    
    if (args.length < 2) {
        console.log(`
批量测试工具 - RainboyOJ客户端

使用方法: node batch_tester.js <目录> <题目ID> [选项]

参数:
  <目录>     包含代码文件的目录
  <题目ID>   要测试的题目ID

选项:
  --host <地址>   服务器地址 (默认: localhost)
  --port <端口>   服务器端口 (默认: 8000)
  --ext <扩展名>  要处理的文件扩展名，逗号分隔 (默认: .cpp,.c,.py)

示例:
  node batch_tester.js ./solutions 1000
  node batch_tester.js ./homework 1001 --host 192.168.1.100 --ext .cpp,.java
        `);
        process.exit(1);
    }
    
    const directory = args[0];
    const problemId = args[1];
    
    // 解析选项
    let host = 'localhost';
    let port = 8000;
    let extensions = ['.cpp', '.c', '.py'];
    
    for (let i = 2; i < args.length; i++) {
        if (args[i] === '--host' && i + 1 < args.length) {
            host = args[++i];
        } else if (args[i] === '--port' && i + 1 < args.length) {
            port = parseInt(args[++i]);
        } else if (args[i] === '--ext' && i + 1 < args.length) {
            extensions = args[++i].split(',');
        }
    }
    
    // 创建过滤函数
    const filterFn = (file) => {
        return extensions.some(ext => file.endsWith(ext));
    };
    
    // 执行批量测试
    console.log(`🚀 开始批量测试，目录: ${directory}，题目ID: ${problemId}`);
    console.log(`📌 服务器: ${host}:${port}，文件类型: ${extensions.join(', ')}\n`);
    
    const tester = new BatchTester(host, port);
    tester.testDirectory(directory, problemId, { filter: filterFn })
        .then(() => console.log('✅ 批量测试完成'))
        .catch(err => {
            console.error('💥 批量测试失败:', err);
            process.exit(1);
        });
}
