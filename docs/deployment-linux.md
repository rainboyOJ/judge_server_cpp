# Linux 部署说明

## 目标

把当前 `judge_server` 原型部署成一个可在 Linux 上监听 TCP 请求、执行 C++/Python 提交并返回 JSON 结果的服务。

## 运行依赖

至少需要：

- `cmake`
- `g++`
- `python3`
- `make` 或 Ninja

推荐额外部署：

- `/usr/bin/sjudge`：提供更接近真实 OJ 的执行与资源限制
- `/judge/checker/fcmp2`：提供输出比较器

如果这两个都没有，服务仍可运行，但会退化到 fallback 执行和内置比较。

## 目录约定

当前代码默认依赖这些路径约定：

- 配置：`config/config.json`
- 测试数据：优先尝试仓库内 `testData/`，其次尝试当前工作目录相对路径和配置项
- checker：`/judge/checker/fcmp2`
- sjudge：`/usr/bin/sjudge`

所以最稳妥的部署方式是保留仓库目录结构运行，或确保这些路径都已经就位。

## 构建主服务

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

启动：

```bash
./build/judge_server config/config.json
```

默认配置示例：

```json
{
  "server": {
    "port": 8000,
    "connection_timeout_ms": 30000
  },
  "testing": {
    "worker_thread_count": 4,
    "max_concurrent_tests": 4,
    "test_data_path": "../testData"
  },
  "performance": {
    "buffer_size": 8192
  }
}
```

## 部署 checker

仓库自带 `checker/` 子项目：

```bash
cd checker
rm -rf build && mkdir build && cd build
cmake ..
make -j
sudo make install
```

安装目标是 `/judge/checker`，其中 `fcmp2` 是当前主流程会优先调用的比较器。

## 部署 sjudge

当前代码只检查 `/usr/bin/sjudge` 是否存在，并通过 `call_sjudge()`（`sjudge/sjudge_call.cpp`）调用它。也就是说：

- sjudge 是外部依赖；
- 没有它时服务仍能跑，但资源限制只是"近似行为"；
- 生产环境如果需要可信资源控制，应先保证 sjudge 已正确安装到 `/usr/bin/sjudge`。

## systemd 示例

```ini
[Unit]
Description=judge_server MVP
After=network.target

[Service]
Type=simple
WorkingDirectory=/opt/boxtest-opencode-dev
ExecStart=/opt/boxtest-opencode-dev/build/judge_server /opt/boxtest-opencode-dev/config/config.json
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
```

部署后建议：

- 把仓库放在稳定路径，例如 `/opt/boxtest-opencode-dev`
- 用非 root 用户运行主服务
- 仅在安装 checker 或 sjudge 时使用 root 权限

## 上线前检查

- `./build/judge_server config/config.json` 能正常启动
- `ctest --test-dir build --output-on-failure` 通过
- `testData/<pid>/data` 已准备好
- `g++` / `python3` 在运行用户环境下可用
- 若需要更真实限制：`/usr/bin/sjudge` 存在
- 若需要更稳定比对：`/judge/checker/fcmp2` 存在

## 当前限制

- 没有内建 daemon 化、日志轮转、指标上报、健康检查接口。
- 没有容器级隔离策略说明；当前文档只覆盖裸 Linux 部署。
- `connection_timeout_ms` 已在配置里出现，但 `TcpServer`/`ClientSockets` 当前没有完整实现基于该值的连接清理逻辑。
