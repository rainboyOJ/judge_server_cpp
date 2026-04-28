# Repository Guidelines

## Project Structure & Module Organization
This repository is a C++17/CMake judge server. Core code lives in `src/` with public headers in `include/`. Main modules are `network/` (TCP server and connection state), `protocol/` (length-prefixed JSON codec), `dispatch/` (queue and worker pool), `pipeline/` (submission, judging, result storage), `runner/` (language runners), and `legacy/` (older compatibility code). The main binary is `judge_server` from `main.cpp`; the shared implementation target is the static library `boxTest`. Tests live in `test/`, local problem data in `testData/`, runtime config in `config/config.json`, and design notes in `docs/` and `doc/`. `checker/` is a separate CMake project.

## Build, Test, and Development Commands
Build locally from the repo root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/judge_server config/config.json
```

Run the full C++ test suite with `ctest --test-dir build --output-on-failure`. Run one test with `ctest --test-dir build -R '^test_submission_service$' --output-on-failure`. Build a single target with `cmake --build build --target judge_server -j`. For the Node protocol helpers, use `cd test/nodejs && npm test`.

## Coding Style & Naming Conventions
Use 4-space indentation and stay within C++17. Match the local style of the file you touch; naming is mixed across the codebase, so avoid renaming for style alone. Prefer small, local diffs, `using` for new aliases, `enum class` for new enums, and concise logging through `LOG_INFO`, `LOG_ERROR`, and `LOG_DEBUG`. Group standard and project includes cleanly. If `clang-format` is available, run it only on touched files.

## Testing Guidelines
Tests are executable-style C++ files under `test/`, and CTest registers them by file stem, for example `test/test_cpp_runner.cpp` becomes `test_cpp_runner`. Add focused regression tests for behavior changes when practical. For protocol or async pipeline changes, run `test_protocol_codec`, `test_submission_service`, `test_judge_worker_pool`, and `test_integration_tcp_cpp_python`.

## Commit & Pull Request Guidelines
Recent history follows Conventional Commit-style prefixes such as `feat:`, `refactor:`, `chore:`, and `docs:`. Keep commits focused on one logical change. Pull requests should explain the behavior change, list the commands/tests you ran, call out config or protocol impacts, and include sample requests/responses when JSON or TCP behavior changes.

## Configuration & Safety Notes
Keep `config/config.json` compatible unless the change is intentional and documented. The framing protocol is `4-byte big-endian length + JSON body`. If `/usr/bin/sjudge` is absent, the fallback runner is less isolated; do not assume production-grade sandboxing in local tests.
