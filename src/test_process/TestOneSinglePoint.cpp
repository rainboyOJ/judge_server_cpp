/**
 * @file TestOneSinglePoint.cpp
 * @brief 旧链路单点执行与新 runner 共用执行 helper 的实现。
 */

#include "common/Logger.h"
#include "legacy/judgeInfo.h"
#include "legacy/resultContainer.h"
#include "sjudge_call.h"
#include "legacy/testBox.h"
#include "test_process/RunnerCompat.h"
#include "utils.h"
#include "legacy/workThreadPool.h"
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char **environ;

namespace fs = std::filesystem;

namespace {

/** @brief 去掉一行右侧多余空白字符。 */
std::string trim_line_right(const std::string &line) {
    size_t end = line.size();
    while (end > 0 && (line[end - 1] == ' ' || line[end - 1] == '\t' ||
                       line[end - 1] == '\r')) {
        --end;
    }
    return line.substr(0, end);
}

/** @brief 归一化输出文件为按行比较的形式。 */
std::vector<std::string> normalize_output_lines(const fs::path &path) {
    std::ifstream stream(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(trim_line_right(line));
    }
    while (!lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }
    return lines;
}

/** @brief 等待子进程结束并提取 signal/exit_code。 */
int wait_and_capture_status(pid_t pid, int &signal, int &exit_code) {
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        return -1;
    }
    signal = 0;
    exit_code = 0;
    if (WIFSIGNALED(status)) {
        signal = WTERMSIG(status);
    } else if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    }
    return status;
}

/**
 * @brief 带真实时间上限地等待子进程结束。
 */
int wait_and_capture_status_with_timeout(pid_t pid, int real_time_limit_ms,
                                         int &signal, int &exit_code,
                                         bool &timed_out) {
    const auto start = std::chrono::steady_clock::now();
    signal = 0;
    exit_code = 0;
    timed_out = false;

    while (true) {
        int status = 0;
        const pid_t wait_result = waitpid(pid, &status, WNOHANG);
        if (wait_result == pid) {
            if (WIFSIGNALED(status)) {
                signal = WTERMSIG(status);
            } else if (WIFEXITED(status)) {
                exit_code = WEXITSTATUS(status);
            }
            return status;
        }
        if (wait_result < 0) {
            return -1;
        }

        if (real_time_limit_ms > 0) {
            const auto now = std::chrono::steady_clock::now();
            const int elapsed_ms = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                      start)
                    .count());
            if (elapsed_ms > real_time_limit_ms) {
                timed_out = true;
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                signal = SIGKILL;
                return status;
            }
        }

        usleep(1000);
    }
}

/** @brief 以重定向 stdin/stdout 的方式启动子进程。 */
bool spawn_with_redirects(const fs::path &program_path,
                          const fs::path &stdin_path,
                          const fs::path &stdout_path, pid_t &child_pid) {
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        return false;
    }

    const int input_fd = open(stdin_path.c_str(), O_RDONLY);
    if (input_fd < 0) {
        posix_spawn_file_actions_destroy(&actions);
        return false;
    }

    const int output_fd =
        open(stdout_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd < 0) {
        close(input_fd);
        posix_spawn_file_actions_destroy(&actions);
        return false;
    }

    posix_spawn_file_actions_adddup2(&actions, input_fd, STDIN_FILENO);
    posix_spawn_file_actions_adddup2(&actions, output_fd, STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, input_fd);
    posix_spawn_file_actions_addclose(&actions, output_fd);

    char *const argv[] = {const_cast<char *>(program_path.c_str()), nullptr};
    const int spawn_rc = posix_spawn(&child_pid, program_path.c_str(), &actions,
                                     nullptr, argv, environ);

    close(input_fd);
    close(output_fd);
    posix_spawn_file_actions_destroy(&actions);
    return spawn_rc == 0;
}

/** @brief 把 sjudge 的 result code 映射成内部 verdict。 */
SubmissionVerdict map_judge_result_to_verdict(int result_code) {
    switch (result_code) {
    case 0:
        return SubmissionVerdict::AC;
    case 1:
    case 2:
        return SubmissionVerdict::TLE;
    case 3:
        return SubmissionVerdict::MLE;
    case 4:
        return SubmissionVerdict::RE;
    default:
        return SubmissionVerdict::UNKNOWN;
    }
}

/** @brief 把新 verdict 映射回旧链路使用的 enum_testStatus。 */
enum_testStatus to_legacy_status(SubmissionVerdict verdict) {
    switch (verdict) {
    case SubmissionVerdict::AC:
        return enum_testStatus::AC;
    case SubmissionVerdict::WA:
    case SubmissionVerdict::PE:
        return enum_testStatus::WA;
    case SubmissionVerdict::TLE:
        return enum_testStatus::TLE;
    case SubmissionVerdict::MLE:
        return enum_testStatus::MLE;
    case SubmissionVerdict::RE:
        return enum_testStatus::RE;
    case SubmissionVerdict::CE:
        return enum_testStatus::CE;
    default:
        return enum_testStatus::UNKNOWN;
    }
}

} // namespace

/** @copydoc compare_case_output */
SubmissionVerdict compare_case_output(const fs::path &input_path,
                                      const fs::path &expected_output,
                                      const fs::path &user_output) {
    const fs::path checkerPath = "/judge/checker/fcmp2";
    if (fs::exists(checkerPath)) {
        const std::string checkerCmd =
            checkerPath.string() + " " + input_path.string() + " " +
            user_output.string() + " " + expected_output.string();
        return system(checkerCmd.c_str()) == 0 ? SubmissionVerdict::AC
                                               : SubmissionVerdict::WA;
    }

    return normalize_output_lines(expected_output) ==
                   normalize_output_lines(user_output)
               ? SubmissionVerdict::AC
               : SubmissionVerdict::WA;
}

/** @copydoc run_executable_case */
RunnerCaseResult run_executable_case(const fs::path &executable_path,
                                     const RunnerCaseInput &test_case,
                                     const fs::path &user_output_path) {
    RunnerCaseResult case_result{};
    case_result.result.seq_id = test_case.seq_id;

    if (!fs::exists(executable_path) || !fs::exists(test_case.input_path) ||
        !fs::exists(test_case.expected_output_path)) {
        case_result.result.verdict = SubmissionVerdict::SYSTEM_ERROR;
        case_result.message = "missing executable or case files";
        return case_result;
    }

    const fs::path sjudgePath = "/usr/bin/sjudge";
    if (fs::exists(sjudgePath)) {
        judge_config config;
        config.max_cpu_time = test_case.cpu_time_limit_ms > 0
                                  ? test_case.cpu_time_limit_ms
                                  : UNLIMITED;
        config.max_real_time = test_case.real_time_limit_ms > 0
                                   ? test_case.real_time_limit_ms
                                   : UNLIMITED;
        config.max_memory = test_case.memory_limit_kb > 0
                                ? test_case.memory_limit_kb
                                : UNLIMITED;
        config.cwd = executable_path.parent_path().string();
        config.exe_path = executable_path.string();
        config.input_path = test_case.input_path;
        config.output_path = user_output_path.string();

        try {
            const judge_result result = call_sjudge(sjudgePath.c_str(), config);
            case_result.result.cpu_time_ms = result.cpu_time;
            case_result.result.real_time_ms = result.real_time;
            case_result.result.memory_kb = result.memory;
            case_result.result.signal = result.signal;
            case_result.result.exit_code = result.exit_code;
            case_result.result.error_code = result.error;
            case_result.result.verdict =
                map_judge_result_to_verdict(result.result);
        } catch (const std::exception &) {
            case_result.result.verdict = SubmissionVerdict::SYSTEM_ERROR;
            case_result.message = "failed to execute sjudge";
            return case_result;
        }
    } else {
        const auto start = std::chrono::steady_clock::now();
        pid_t child_pid = -1;
        const bool spawned = spawn_with_redirects(
            executable_path, test_case.input_path, user_output_path, child_pid);
        int signal = 0;
        int exit_code = 0;
        bool timed_out = false;
        const int raw_status =
            spawned ? wait_and_capture_status_with_timeout(
                          child_pid, test_case.real_time_limit_ms, signal,
                          exit_code, timed_out)
                    : -1;
        const auto end = std::chrono::steady_clock::now();

        case_result.result.real_time_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count());
        case_result.result.cpu_time_ms = case_result.result.real_time_ms;

        if (!spawned || raw_status == -1) {
            case_result.result.verdict = SubmissionVerdict::SYSTEM_ERROR;
            case_result.message = "failed to start executable";
            return case_result;
        }

        if (timed_out) {
            case_result.result.signal = signal;
            case_result.result.verdict = SubmissionVerdict::TLE;
            case_result.message = "executable exceeded real time limit";
            return case_result;
        }

        if (signal != 0) {
            case_result.result.signal = signal;
            case_result.result.verdict = SubmissionVerdict::RE;
            case_result.message = "executable terminated by signal";
            return case_result;
        }

        case_result.result.exit_code = exit_code;
        if (exit_code != 0) {
            case_result.result.verdict = SubmissionVerdict::RE;
            case_result.message = "executable returned non-zero exit code";
            return case_result;
        }

        case_result.result.verdict = SubmissionVerdict::AC;
    }

    if (case_result.result.verdict == SubmissionVerdict::AC) {
        case_result.result.verdict = compare_case_output(
            test_case.input_path, test_case.expected_output_path,
            user_output_path);
        case_result.message =
            case_result.result.verdict == SubmissionVerdict::AC
                ? user_output_path.string()
                : "output mismatch";
    }

    return case_result;
}

/**
 * @brief 旧链路中的单点测试入口。
 *
 * @note 当前异步主链路优先通过 runner + run_executable_case 执行，
 *       本函数主要保留给旧 testBox/workThreadPool 路径。
 */
bool TestOneSinglePoint(const int testBoxId, int seq_id,
                        resultContainer *resultContainerPtr) {
    LOG_DEBUG("TestOneSinglePoint Start, testBoxId %d, seq_id %d", testBoxId,
              seq_id);

    // 获取题目ID
    char pid[32];
    strcpy(pid, resultContainerPtr->getPid(testBoxId));

    auto testBoxPtr = resultContainerPtr->getTestBoxPtr();
    auto problemPath = testBoxPtr->getProbelmPath();

    // 获取可执行文件路径（假设在预处理阶段已编译好）
    fs::path tempDir = resultContainerPtr->work_dir(testBoxId);
    fs::path sourceFile = resultContainerPtr->source_path(testBoxId);
    fs::path executableFile = resultContainerPtr->exe_path(testBoxId);

    if (!fs::exists(executableFile)) {
        LOG_ERROR("Executable file not found for testBoxId %d", testBoxId);
        return false;
    }

    // 获取测试用例信息
    TestCaseInfo testCaseInfo;
    if (!resultContainerPtr->getTestCaseInfo(testBoxId, seq_id, testCaseInfo)) {
        LOG_ERROR("Failed to get test case info for testBoxId %d, seq_id %d",
                  testBoxId, seq_id);
        return false;
    }

    RunnerCaseInput case_input{};
    case_input.seq_id = seq_id;
    case_input.input_path = testCaseInfo.input_path;
    case_input.expected_output_path = testCaseInfo.output_path;
    case_input.cpu_time_limit_ms = testCaseInfo.cpu_time_limit;
    case_input.real_time_limit_ms = testCaseInfo.real_time_limit;
    case_input.memory_limit_kb = testCaseInfo.memory_limit;

    const RunnerCaseResult case_result = run_executable_case(
        executableFile, case_input, testCaseInfo.user_output_path);

    TestCaseResult testCaseResult;
    testCaseResult.testBoxId = testBoxId;
    testCaseResult.seq_id = seq_id;
    testCaseResult.cpu_time = case_result.result.cpu_time_ms;
    testCaseResult.real_time = case_result.result.real_time_ms;
    testCaseResult.memory = case_result.result.memory_kb;
    testCaseResult.signal = case_result.result.signal;
    testCaseResult.exit_code = case_result.result.exit_code;
    testCaseResult.error = case_result.result.error_code;
    testCaseResult.result = to_legacy_status(case_result.result.verdict);

    resultContainerPtr->writeCaseResult(testBoxId, seq_id, testCaseResult);

    return case_result.result.verdict == SubmissionVerdict::AC;

    return true;
}
