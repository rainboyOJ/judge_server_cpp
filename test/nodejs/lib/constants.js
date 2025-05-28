/**
 * RainboyOJ 常量定义模块
 * 包含配置常量和枚举类型
 */

// 默认配置常量
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

module.exports = {
    CONFIG,
    Language,
    TestError
};
