#!/usr/bin/env node

/**
 * 演示脚本 - 展示不同的使用方式
 */

const { exec } = require('child_process');
const path = require('path');

console.log('🚀 RainboyOJ Node.js 客户端演示\n');

// 演示用例
const demos = [
    {
        title: '基本使用 - 默认A+B题目',
        command: 'node send_one_judge.js --host 127.0.0.1 --port 8000'
    },
    {
        title: '从文件读取C++代码',
        command: 'node send_one_judge.js --file examples/a_plus_b.cpp --lang cpp --problem 1000'
    },
    {
        title: '从文件读取Python代码',
        command: 'node send_one_judge.js --file examples/hello.py --lang python --problem hello'
    },
    {
        title: '从文件读取C代码',
        command: 'node send_one_judge.js --file examples/hello.c --lang c --problem hello'
    },
    {
        title: '直接指定代码内容',
        command: 'node send_one_judge.js --code "print(\\"Hello from inline code!\\")" --lang python'
    }
];

async function runDemo(demo, index) {
    console.log(`\n📋 演示 ${index + 1}: ${demo.title}`);
    console.log(`命令: ${demo.command}`);
    console.log('─'.repeat(60));
    
    return new Promise((resolve) => {
        const child = exec(demo.command, { 
            cwd: __dirname,
            timeout: 10000 // 10秒超时
        });
        
        child.stdout.on('data', (data) => {
            process.stdout.write(data);
        });
        
        child.stderr.on('data', (data) => {
            process.stderr.write(data);
        });
        
        child.on('close', (code) => {
            console.log(`\n─'.repeat(60)`);
            console.log(`示例 ${index + 1} ${code === 0 ? '成功完成' : '执行失败'}`);
            console.log('─'.repeat(60));
            resolve();
        });
    });
}

async function runAllDemos() {
    for (let i = 0; i < demos.length; i++) {
        try {
            await runDemo(demos[i], i);
            // 演示之间暂停1秒
            if (i < demos.length - 1) {
                await new Promise(resolve => setTimeout(resolve, 1000));
            }
        } catch (error) {
            console.error(`演示 ${i + 1} 失败:`, error);
        }
    }
    console.log('\n🎉 所有演示完成!');
}

runAllDemos().catch(err => {
    console.error('演示运行失败:', err);
    process.exit(1);
});
