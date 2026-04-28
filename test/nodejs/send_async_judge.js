#!/usr/bin/env node

const net = require('net');
const fs = require('fs');
const path = require('path');
const { CONFIG, Language } = require('./lib/constants');

function parseArgs(args = process.argv.slice(2)) {
  const options = {
    host: CONFIG.host,
    port: CONFIG.port,
    problemId: '1000',
    language: Language.CPP,
    code: `#include <iostream>
using namespace std;

int main() {
    int a, b;
    cin >> a >> b;
    if( a % 2 == 0)
        a ++;
    cout << a + b  << endl;
    return 0;
}`,
    uuid: Math.floor(Math.random() * 1000000),
    pollMs: 300,
    maxPolls: 40,
  };

  for (let i = 0; i < args.length; i++) {
    switch (args[i]) {
      case '--host':
        options.host = args[++i];
        break;
      case '--port':
        options.port = parseInt(args[++i], 10);
        break;
      case '--problem':
      case '--pid':
        options.problemId = args[++i];
        break;
      case '--lang': {
        const langMap = { cpp: Language.CPP, c: Language.C, python: Language.PYTHON };
        options.language = langMap[args[++i]] ?? Language.CPP;
        break;
      }
      case '--code':
        options.code = args[++i];
        break;
      case '--file': {
        const filePath = args[++i];
        options.code = fs.readFileSync(filePath, 'utf8');
        break;
      }
      case '--uuid':
        options.uuid = parseInt(args[++i], 10);
        break;
      case '--poll-ms':
        options.pollMs = parseInt(args[++i], 10);
        break;
      case '--max-polls':
        options.maxPolls = parseInt(args[++i], 10);
        break;
    }
  }

  return options;
}

function encodeFrame(jsonObject) {
  const payload = Buffer.from(JSON.stringify(jsonObject), 'utf8');
  const header = Buffer.allocUnsafe(4);
  header.writeUInt32BE(payload.length, 0);
  return Buffer.concat([header, payload]);
}

function createFrameReader(socket) {
  let buffer = Buffer.alloc(0);
  const pending = [];
  let waiter = null;

  const drain = () => {
    while (buffer.length >= 4) {
      const len = buffer.readUInt32BE(0);
      if (buffer.length < 4 + len) {
        return;
      }
      const payload = buffer.subarray(4, 4 + len);
      buffer = buffer.subarray(4 + len);
      const parsed = JSON.parse(payload.toString('utf8'));
      if (waiter) {
        const resolve = waiter;
        waiter = null;
        resolve(parsed);
      } else {
        pending.push(parsed);
      }
    }
  };

  socket.on('data', (chunk) => {
    buffer = Buffer.concat([buffer, chunk]);
    drain();
  });

  return async function readOneMessage() {
    if (pending.length > 0) {
      return pending.shift();
    }
    return new Promise((resolve) => {
      waiter = resolve;
    });
  };
}

async function connect(host, port) {
  const socket = new net.Socket();
  await new Promise((resolve, reject) => {
    socket.once('error', reject);
    socket.connect(port, host, resolve);
  });
  return socket;
}

async function sendAndReadOne(host, port, requestObject) {
  const socket = await connect(host, port);
  const readOneMessage = createFrameReader(socket);
  socket.write(encodeFrame(requestObject));
  const response = await readOneMessage();
  socket.destroy();
  return response;
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function main() {
  const options = parseArgs();

  const submitRequest = {
    type: 'submit',
    uuid: options.uuid,
    pid: options.problemId,
    lang: options.language,
    code: options.code,
  };

  console.log('🚀 提交异步评测请求');
  console.log(JSON.stringify(submitRequest, null, 2));

  const ack = await sendAndReadOne(options.host, options.port, submitRequest);
  console.log('\n📥 收到第一条响应');
  console.log(JSON.stringify(ack, null, 2));

  if (ack.type !== 'submission_ack') {
    console.error('❌ 第一条响应不是 submission_ack，停止');
    process.exit(1);
  }

  const submissionId = ack.submission_id;

  for (let i = 0; i < options.maxPolls; i++) {
    await sleep(options.pollMs);
    const queryRequest = { type: 'query_result', submission_id: submissionId };
    const queried = await sendAndReadOne(options.host, options.port, queryRequest);

    console.log(`\n🔎 第 ${i + 1} 次查询结果`);
    console.log(JSON.stringify(queried, null, 2));

    if (queried.type === 'submission_finished' || queried.status === 'FINISHED' || queried.status === 'FAILED') {
      console.log('\n🎉 评测流程结束');
      return;
    }
  }

  console.log('\n⚠️ 已达到最大轮询次数，评测可能仍在运行');
}

main().catch((error) => {
  console.error('💥 异步前端脚本失败:', error.message);
  process.exit(1);
});
