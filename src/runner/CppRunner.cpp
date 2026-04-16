/**
 * @file CppRunner.cpp
 * @brief C++ 提交执行器实现。
 */

#include "runner/CppRunner.h"

#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <spawn.h>
#include <sstream>
#include <string>
#include <system_error>

#include <sys/resource.h>

#include "test_process/RunnerCompat.h"

namespace fs = std::filesystem;

namespace {

/** @brief 根据 submission 计算 C++ runner 的工作目录。 */
fs::path work_dir_for(const SubmissionRequest &request) {
    return fs::temp_directory_path() /
           ("oj_compile_" + std::to_string(request.uuid));
}

/** @brief C++ 源文件路径。 */
fs::path source_path_for(const SubmissionRequest &request) {
    return work_dir_for(request) / "solution.cpp";
}

/** @brief 编译后的可执行文件路径。 */
fs::path executable_path_for(const SubmissionRequest &request) {
    return work_dir_for(request) / "solution";
}

/** @brief 编译日志文件路径。 */
fs::path compile_log_path_for(const SubmissionRequest &request) {
    return work_dir_for(request) / "compile.log";
}

/** @brief 单个测试点的用户输出文件路径。 */
fs::path user_output_path_for(const SubmissionRequest &request,
                              const RunnerCaseInput &test_case) {
    return work_dir_for(request) /
           ("case_" + std::to_string(test_case.seq_id) + ".user.out");
}

/** @brief 从文件读取完整文本内容。 */
std::string read_file(const fs::path &path) {
    std::ifstream stream(path);
    if (!stream.good()) {
        return "";
    }

    return std::string((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());
}

/** @brief 判断 CppRunner 执行所需的最小请求字段是否齐备。 */
bool has_basic_request_fields(const SubmissionRequest &request) {
    return request.uuid > 0 && !request.code.empty();
}

} // namespace

/** @copydoc ILanguageRunner::prepare */
RunnerPrepareResult CppRunner::prepare(const SubmissionRequest &request) {
    if (!has_basic_request_fields(request)) {
        return RunnerPrepareResult{false, "cpp runner requires uuid and code"};
    }

    const fs::path work_dir = work_dir_for(request);
    const fs::path source_path = source_path_for(request);

    std::error_code ec;
    fs::remove_all(work_dir, ec);
    ec.clear();
    fs::create_directories(work_dir, ec);
    if (ec) {
        return RunnerPrepareResult{false,
                                   "failed to create cpp runner work dir"};
    }

    std::ofstream source_stream(source_path);
    if (!source_stream.good()) {
        return RunnerPrepareResult{false, "failed to write cpp source file"};
    }

    // 中文注释：这里故意把 runner 工作目录固定到 temp 路径，
    // 避免提前绑定旧 test_process 的 resultContainer 生命周期。
    source_stream << request.code;
    source_stream.close();

    return RunnerPrepareResult{true, source_path.string()};
}

/** @copydoc ILanguageRunner::compile */
RunnerCompileResult CppRunner::compile(const SubmissionRequest &request) {
    if (!has_basic_request_fields(request)) {
        return RunnerCompileResult{false, SubmissionVerdict::SYSTEM_ERROR,
                                   "cpp runner requires uuid and code"};
    }

    const fs::path source_path = source_path_for(request);
    const fs::path executable_path = executable_path_for(request);
    const fs::path compile_log_path = compile_log_path_for(request);

    if (!fs::exists(source_path)) {
        return RunnerCompileResult{false, SubmissionVerdict::SYSTEM_ERROR,
                                   "cpp source file not prepared"};
    }

    std::string compile_output;
    if (compile_cpp_source_file(source_path, executable_path, compile_log_path,
                                compile_output) &&
        fs::exists(executable_path)) {
        return RunnerCompileResult{true, SubmissionVerdict::AC,
                                   executable_path.string()};
    }

    const std::string compile_message = read_file(compile_log_path);
    std::error_code ec;
    fs::remove_all(work_dir_for(request), ec);
    return RunnerCompileResult{false, SubmissionVerdict::CE,
                               compile_message.empty() ? compile_output
                                                       : compile_message};
}

/** @copydoc ILanguageRunner::runCase */
RunnerCaseResult CppRunner::runCase(const SubmissionRequest &request,
                                    const RunnerCaseInput &test_case) {
    RunnerCaseResult case_result{};
    case_result.result.seq_id = test_case.seq_id;

    if (!has_basic_request_fields(request)) {
        case_result.result.verdict = SubmissionVerdict::SYSTEM_ERROR;
        case_result.message = "cpp runner requires uuid and code";
        return case_result;
    }

    const fs::path executable_path = executable_path_for(request);
    const fs::path input_path = test_case.input_path;
    const fs::path expected_output_path = test_case.expected_output_path;
    const fs::path user_output_path = user_output_path_for(request, test_case);

    if (!fs::exists(executable_path) || !fs::exists(input_path) ||
        !fs::exists(expected_output_path)) {
        case_result.result.verdict = SubmissionVerdict::SYSTEM_ERROR;
        case_result.message = "cpp runner missing executable or case files";
        return case_result;
    }

    case_result =
        run_executable_case(executable_path, test_case, user_output_path);
    return case_result;
}
