#!/usr/bin/env node

/**
 * 编码解码测试脚本
 * 测试 DataCodec 类的序列化和反序列化功能
 */

const { DataCodec, Language, TestError } = require('./send_one_judge.js');

console.log('🧪 开始编码解码测试...\n');

// 测试数据
const testProblem = {
    uuid: 12345,
    pid: '1000',
    language: Language.CPP,
    code: `#include <iostream>
using namespace std;

int main() {
    cout << "Hello World!" << endl;
    return 0;
}`
};

console.log('📝 原始测试数据:');
console.log(JSON.stringify(testProblem, null, 2));
console.log();

try {
    // 测试序列化
    console.log('🔧 开始序列化...');
    const serializedData = DataCodec.serializeTestProblem(testProblem);
    
    console.log(`✅ 序列化完成，数据长度: ${serializedData.length} 字节`);
    console.log('📦 序列化数据 (Hex):');
    console.log(serializedData.toString('hex'));
    console.log();
    
    // 测试反序列化
    console.log('🔧 开始反序列化...');
    const deserializedData = DataCodec.deserializeTestProblem(serializedData);
    
    console.log('✅ 反序列化完成');
    console.log('📋 反序列化结果:');
    console.log(JSON.stringify(deserializedData, null, 2));
    console.log();
    
    // 验证数据一致性
    console.log('🔍 验证数据一致性...');
    const isValid = (
        testProblem.uuid === deserializedData.uuid &&
        testProblem.pid === deserializedData.pid &&
        testProblem.language === deserializedData.language &&
        testProblem.code === deserializedData.code
    );
    
    if (isValid) {
        console.log('✅ 数据一致性验证通过！');
    } else {
        console.log('❌ 数据一致性验证失败！');
        console.log('差异对比:');
        Object.keys(testProblem).forEach(key => {
            if (testProblem[key] !== deserializedData[key]) {
                console.log(`  ${key}: 原始="${testProblem[key]}" vs 反序列化="${deserializedData[key]}"`);
            }
        });
    }
    
} catch (error) {
    console.error('❌ 测试失败:', error.message);
    console.error(error.stack);
    process.exit(1);
}

console.log('\n🎉 编码解码测试完成！');
