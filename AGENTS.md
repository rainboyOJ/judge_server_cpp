# AGENTS.md
Instructions for coding agents working in this repository.
Scope: `/home/rainboy/mycode/boxtest-opencode-dev`.

## Project at a glance
- Language/toolchain: C++17 + CMake.
- Main binary: `judge_server`.
- Core target: static lib `boxTest`.
- Current architecture is transitional: legacy TCP/event loop shell with a new judge pipeline behind it.
- New judge pipeline layers: `protocol -> service -> runner -> judge -> store`.
- Tests: executable-style C++ tests under `test/`, registered via CTest.
- Extra modules:
  - `checker/` (independent CMake project for checkers).
  - `test/nodejs/` (Node protocol/client scripts).

## Key paths
- `src/` implementation
- `include/` headers
- `src/common/`, `include/common/` shared components (logger/config/result/types)
- `src/protocol/`, `include/protocol/` JSON protocol codec
- `src/service/`, `include/service/` submission orchestration
- `src/runner/`, `include/runner/` language runners (`C++`, `Python`)
- `src/judge/`, `include/judge/` verdict aggregation
- `src/store/`, `include/store/` submission state storage
- `src/test_process/`, `include/test_process/` compatibility helpers reused by runners
- `test/` C++ tests and CTest registration
- `testData/` local problem data used by service/integration tests
- `config/config.json` runtime config
- `doc/`, `docs/` design/protocol notes

## Build commands (C++)
From repository root.

Configure + build Debug:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

Configure + build Release:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Build one target:
```bash
cmake --build build --target judge_server -j
cmake --build build --target test_submission_service -j
cmake --build build --target test_integration_tcp_cpp_python -j
```

Run server:
```bash
./build/judge_server
./build/judge_server config/config.json
```

## Test commands (especially single test)
Run all tests:
```bash
ctest --test-dir build --output-on-failure
```

Run one test by exact CTest name (preferred):
```bash
ctest --test-dir build -R '^test_submission_service$' --output-on-failure
ctest --test-dir build -R '^test_integration_tcp_cpp_python$' --output-on-failure
```

Run a subset by regex:
```bash
ctest --test-dir build -R 'test_(cpp|python)_runner' --output-on-failure
```

List test names:
```bash
ctest --test-dir build -N
```

Run one test executable directly:
```bash
./build/test/test_submission_service
./build/test/test_integration_tcp_cpp_python
```

If CTest references stale absolute paths (repo moved), regenerate:
```bash
cmake -S . -B build
```

## Lint / formatting / static analysis
- No dedicated top-level lint target is configured.
- Formatting baseline from `.clang-format` (`IndentWidth: 4`).
- Format touched files manually, e.g.:
```bash
clang-format -i src/file.cpp include/file.h test/test_x.cpp
```
- Avoid repo-wide reformat unless requested.
- Optional but not wired by default: `clang-tidy`, `cppcheck`.

## Node client commands (`test/nodejs`)
From `test/nodejs/`:
```bash
npm install
npm test
npm run demo
npm run judge
```
Run single script:
```bash
node test_codec.js
node send_one_judge.js
```
Notes:
- `npm test` currently verifies codec/framing behavior.
- `npm run judge` requires a running server on the configured port.

## Code style and implementation rules

### General
- Prefer minimal, local diffs; avoid broad refactors unless required.
- Preserve existing architecture and interfaces unless task asks to change them.
- Keep comments concise; remove stale comments when editing nearby code.
- Avoid adding dependencies unless clearly needed.

### Includes/imports
- Group standard headers and project headers clearly.
- Prefer matching header first in `.cpp` files when practical.
- Do not introduce duplicate includes.
- Prefer forward declarations in headers when they reduce coupling.

### Formatting
- 4-space indentation.
- Match surrounding brace/spacing style in touched files.
- Avoid formatting-only noise outside changed logic.

### Types
- Stay within C++17 unless explicitly asked otherwise.
- Prefer `using` over `typedef` for new aliases.
- Prefer `enum class` for new enums (legacy plain enums can stay).
- Use `const` / `constexpr` for immutable values.
- Use fixed-width integer types for protocol/binary-sensitive fields.

### Naming
- Existing style is mixed (`camelCase`, `snake_case`, lower-case class names like `testBox`).
- Follow local style of the file/module you modify.
- Do not mass-rename identifiers just for consistency.
- Test executable names are derived from `test/*.cpp` file stems.

### Error handling
- Use explicit return/status checks for expected failures.
- Throw exceptions for invariants/unexpected failures.
- Catch specific exceptions where possible (`json::exception`, `std::out_of_range`, etc.).
- Keep failure logs actionable (include ids, paths, operation).
- Preserve existing failure propagation patterns in each module.

### Concurrency
- Preserve current thread-safety assumptions in shared components.
- Keep lock scope tight (`std::mutex` + `std::lock_guard` is current pattern).
- Do not hold locks across blocking I/O or long-running operations.
- TCP path now uses background submission threads; avoid reintroducing blocking work on the select loop.
- Runner working directories must use service-generated ids, not client-supplied ids.

### Logging
- Use existing macros: `LOG_INFO`, `LOG_ERROR`, `LOG_DEBUG`, `LOG_FATAL`.
- `LOG_DEBUG` depends on `MUDEBUG` (enabled by Debug build config path).
- Prefer short, structured messages with stable keywords.
- Do not leave `std::cout` debug prints in socket/server code.

### JSON/config
- JSON stack: `nlohmann::json` via `include/json.hpp`.
- Keep config compatibility with `config/config.json` unless migration is requested.
- Provide sensible defaults for missing config keys.
- Protocol framing is `4-byte big-endian length + JSON body`.

### Judge pipeline notes
- `JudgeProtocol` handles JSON decode/encode only.
- `SubmissionService` orchestrates `prepare -> compile/check -> run cases -> aggregate`.
- `RunnerFactory` currently supports `C++` and `Python`; `C` remains unsupported.
- `JudgeCore` decides final verdict from case results.
- `ResultStore` persists forward-only state transitions.

### Testing expectations
- For behavior changes, add or update focused tests under `test/` when feasible.
- Run targeted test(s) first, then broader suite as needed.
- For bug fixes, add a regression test when practical.
- For protocol or TCP changes, run `test_protocol_codec`, `test_submission_service`, and `test_integration_tcp_cpp_python`.
- For runner changes, run `test_cpp_runner` and/or `test_python_runner`.

## Current limitations to respect
- Fallback execution path (when `/usr/bin/sjudge` is absent) only has basic real-time timeout handling; it is not a full sandbox.
- Checker fallback compares normalized text and is less powerful than production checker semantics.
- Socket layer is transitionary: JSON requests use the new pipeline, legacy result path still exists for old flow.
- Node scripts are partially integration helpers; some require a separately running server.

## Cursor/Copilot rule files
Checked and not found at time of writing:
- `.cursor/rules/`
- `.cursorrules`
- `.github/copilot-instructions.md`
If any appear later, treat them as higher-priority repository instructions.

## Recommended agent workflow
- Inspect impacted `CMakeLists.txt` and related headers before edits.
- After edits: run build + targeted tests at minimum.
- For networking/threading/serialization changes: run full CTest suite.
- For protocol/client changes: also run `npm test` in `test/nodejs`.
- Keep commits focused: one logical change per commit.
