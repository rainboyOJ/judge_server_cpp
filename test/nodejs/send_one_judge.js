#!/usr/bin/env node

/**
 * RainboyOJ 评测客户端 - Node.js版本
 * 基于lib库实现的评测请求工具
 */

const {
    CONFIG,
    Language,
    TestError,
    JudgeClient,
    parseArguments,
    showHelp
} = require('./lib/index.js');

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
            lang: options.language,
            code: options.code
        };
        
        // 创建客户端并发送请求
        const client = new JudgeClient(options.host, options.port);
        const result = await client.sendJudgeRequest(testProblem);
        
        // 输出摘要信息
        console.log('🎉 评测完成!');
        // console.log(result);
        
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
