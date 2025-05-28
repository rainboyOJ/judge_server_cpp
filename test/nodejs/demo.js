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
            console.log(`\n✅ 演示完成 (退出码: ${code})`);
            resolve();
        });
        
        child.on('error', (error) => {
            console.error(`❌ 执行错误: ${error.message}`);
            resolve();
        });
    });
}

async function main() {
    console.log('⚠️  注意: 这些演示需要评测服务器运行在 localhost:8000');
    console.log('如果服务器未运行，演示将显示连接错误，这是正常的。\n');
    
    // 只演示编码解码功能，不实际连接服务器
    console.log('🧪 首先测试编码解码功能:');
    await runDemo({ command: 'node test_codec.js' }, -1);
    
    console.log('\n📚 以下是各种用法的命令演示 (不会实际执行):');
    
    demos.forEach((demo, index) => {
        console.log(`\n${index + 1}. ${demo.title}`);
        console.log(`   ${demo.command}`);
    });
    
    console.log('\n💡 使用提示:');
    console.log('• 使用 --help 查看完整帮助信息');
    console.log('• 使用 --host 和 --port 指定服务器地址');
    console.log('• 支持从文件读取代码或直接传入代码字符串');
    console.log('• 支持 cpp, c, python 三种语言');
    console.log('• 所有接收到的数据都会详细显示，便于调试');
    
    console.log('\n🎉 演示完成！');
}

main().catch(console.error);
