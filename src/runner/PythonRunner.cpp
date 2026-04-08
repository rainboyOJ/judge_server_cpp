#include "runner/PythonRunner.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include "test_process/RunnerCompat.h"

namespace fs = std::filesystem;

namespace {

fs::path work_dir_for(const SubmissionRequest &request) {
    return fs::temp_directory_path() /
           ("oj_compile_" + std::to_string(request.uuid));
}

fs::path source_path_for(const SubmissionRequest &request) {
    return work_dir_for(request) / "solution.py";
}

fs::path compile_log_path_for(const SubmissionRequest &request) {
    return work_dir_for(request) / "compile.log";
}

fs::path user_output_path_for(const SubmissionRequest &request,
                              const RunnerCaseInput &test_case) {
    return work_dir_for(request) /
           ("case_" + std::to_string(test_case.seq_id) + ".user.out");
}

fs::path wrapper_path_for(const SubmissionRequest &request) {
    return work_dir_for(request) / "run_python.sh";
}

bool has_basic_request_fields(const SubmissionRequest &request) {
    return request.uuid > 0 && !request.code.empty();
}

std::string read_file(const fs::path &path) {
    std::ifstream stream(path);
    if (!stream.good()) {
        return "";
    }

    return std::string((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());
}

std::string shell_escape_single_quotes(const std::string &value) {
    std::string quoted;
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted += ch;
        }
    }
    return quoted;
}

} // namespace

RunnerPrepareResult PythonRunner::prepare(const SubmissionRequest &request) {
    if (!has_basic_request_fields(request)) {
        return RunnerPrepareResult{false,
                                   "python runner requires uuid and code"};
    }

    const fs::path work_dir = work_dir_for(request);
    const fs::path source_path = source_path_for(request);

    std::error_code ec;
    fs::remove_all(work_dir, ec);
    ec.clear();
    fs::create_directories(work_dir, ec);
    if (ec) {
        return RunnerPrepareResult{false,
                                   "failed to create python runner work dir"};
    }

    std::ofstream source_stream(source_path);
    if (!source_stream.good()) {
        return RunnerPrepareResult{false, "failed to write python source file"};
    }

    source_stream << request.code;
    source_stream.close();
    return RunnerPrepareResult{true, source_path.string()};
}

RunnerCompileResult PythonRunner::compile(const SubmissionRequest &request) {
    if (!has_basic_request_fields(request)) {
        return RunnerCompileResult{false, SubmissionVerdict::SYSTEM_ERROR,
                                   "python runner requires uuid and code"};
    }

    const fs::path source_path = source_path_for(request);
    const fs::path compile_log_path = compile_log_path_for(request);
    if (!fs::exists(source_path)) {
        return RunnerCompileResult{false, SubmissionVerdict::SYSTEM_ERROR,
                                   "python source file not prepared"};
    }

    // 中文注释：Python 没有像 C++ 那样的独立二进制编译产物，
    // 这里用 py_compile 做语法检查，把解释型语言统一映射到“编译阶段”。
    const fs::path pycache_path = work_dir_for(request) / "__pycache__";
    const std::string command =
        "python3 -m py_compile '" +
        shell_escape_single_quotes(source_path.string()) + "' > '" +
        shell_escape_single_quotes(compile_log_path.string()) + "' 2>&1";
    const int status = std::system(command.c_str());
    if (status == 0 || fs::exists(pycache_path)) {
        return RunnerCompileResult{true, SubmissionVerdict::AC,
                                   source_path.string()};
    }

    return RunnerCompileResult{false, SubmissionVerdict::CE,
                               read_file(compile_log_path)};
}

RunnerCaseResult PythonRunner::runCase(const SubmissionRequest &request,
                                       const RunnerCaseInput &test_case) {
    RunnerCaseResult case_result{};
    case_result.result.seq_id = test_case.seq_id;

    if (!has_basic_request_fields(request)) {
        case_result.result.verdict = SubmissionVerdict::SYSTEM_ERROR;
        case_result.message = "python runner requires uuid and code";
        return case_result;
    }

    const fs::path source_path = source_path_for(request);
    const fs::path input_path = test_case.input_path;
    const fs::path expected_output_path = test_case.expected_output_path;
    const fs::path user_output_path = user_output_path_for(request, test_case);
    const fs::path wrapper_path = wrapper_path_for(request);

    if (!fs::exists(source_path) || !fs::exists(input_path) ||
        !fs::exists(expected_output_path)) {
        case_result.result.verdict = SubmissionVerdict::SYSTEM_ERROR;
        case_result.message = "python runner missing source or case files";
        return case_result;
    }

    std::ofstream wrapper_stream(wrapper_path);
    if (!wrapper_stream.good()) {
        case_result.result.verdict = SubmissionVerdict::SYSTEM_ERROR;
        case_result.message = "failed to create python wrapper script";
        return case_result;
    }
    // 中文注释：这里生成一个最小 wrapper，把 Python 脚本纳入和 C++
    // 一样的统一执行入口。
    wrapper_stream << "#!/bin/sh\nexec python3 '"
                   << shell_escape_single_quotes(source_path.string()) << "'\n";
    wrapper_stream.close();
    fs::permissions(wrapper_path,
                    fs::perms::owner_read | fs::perms::owner_write |
                        fs::perms::owner_exec,
                    fs::perm_options::replace);

    return run_executable_case(wrapper_path, test_case, user_output_path);
}
