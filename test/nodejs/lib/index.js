/**
 * RainboyOJ Node.js 客户端库
 * 用于与RainboyOJ评测系统通信
 */

const { CONFIG, Language, TestError } = require('./constants');
const DataCodec = require('./data-codec');
const JudgeConnector = require('./judge-connector');
const JudgeClient = require('./judge-client');
const { parseArguments, showHelp } = require('./cli-util');

module.exports = {
    // 常量和枚举
    CONFIG,
    Language,
    TestError,
    
    // 主要类
    DataCodec,
    JudgeConnector,
    JudgeClient,
    
    // 工具函数
    parseArguments,
    showHelp
};
