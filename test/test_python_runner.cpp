#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "runner/PythonRunner.h"

namespace fs = std::filesystem;

namespace {

fs::path make_work_dir(int uuid) {
    return fs::temp_directory_path() / ("oj_compile_" + std::to_string(uuid));
}

void remove_work_dir(int uuid) {
    std::error_code ec;
    fs::remove_all(make_work_dir(uuid), ec);
}

SubmissionRequest make_request(int uuid, const std::string &code) {
    SubmissionRequest request{};
    request.uuid = uuid;
    request.pid = "unit-test";
    request.language = SubmissionLanguage::PYTHON;
    request.code = code;
    return request;
}

std::string read_file(const fs::path &path) {
    std::ifstream stream(path);
    return std::string((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());
}

void write_file(const fs::path &path, const std::string &content) {
    std::ofstream stream(path);
    assert(stream.good());
    stream << content;
}

void test_compile_check_succeeds_for_syntax_valid_python() {
    const int uuid = 72001;
    remove_work_dir(uuid);

    PythonRunner runner;
    const SubmissionRequest request = make_request(
        uuid, "import sys\ntext = sys.stdin.read()\nprint(text.strip())\n");

    const RunnerPrepareResult prepare_result = runner.prepare(request);
    const RunnerCompileResult compile_result = runner.compile(request);

    assert(prepare_result.ok);
    assert(compile_result.ok);
    assert(compile_result.verdict == SubmissionVerdict::AC);
    assert(fs::exists(make_work_dir(uuid) / "solution.py"));

    remove_work_dir(uuid);
}

void test_compile_check_fails_for_syntax_invalid_python() {
    const int uuid = 72002;
    remove_work_dir(uuid);

    PythonRunner runner;
    const SubmissionRequest request =
        make_request(uuid, "def broken(:\n    return 1\n");

    const RunnerPrepareResult prepare_result = runner.prepare(request);
    const RunnerCompileResult compile_result = runner.compile(request);

    assert(prepare_result.ok);
    assert(!compile_result.ok);
    assert(compile_result.verdict == SubmissionVerdict::CE);
    assert(!compile_result.message.empty());

    remove_work_dir(uuid);
}

void test_run_case_returns_ac_for_matching_output() {
    const int uuid = 72003;
    remove_work_dir(uuid);

    PythonRunner runner;
    const SubmissionRequest request = make_request(
        uuid, "import sys\nfor line in sys.stdin:\n    print(line.strip())\n");

    const RunnerPrepareResult prepare_result = runner.prepare(request);
    const RunnerCompileResult compile_result = runner.compile(request);
    assert(prepare_result.ok);
    assert(compile_result.ok);

    const fs::path work_dir = make_work_dir(uuid);
    const fs::path input_path = work_dir / "case.in";
    const fs::path expected_path = work_dir / "case.out";
    write_file(input_path, "hello python\n");
    write_file(expected_path, "hello python\n");

    RunnerCaseInput input{};
    input.seq_id = 1;
    input.input_path = input_path.string();
    input.expected_output_path = expected_path.string();

    const RunnerCaseResult case_result = runner.runCase(request, input);

    assert(case_result.result.seq_id == 1);
    assert(case_result.result.verdict == SubmissionVerdict::AC);
    assert(case_result.result.exit_code == 0);
    assert(read_file(work_dir / "case_1.user.out") == "hello python\n");

    remove_work_dir(uuid);
}

void test_run_case_maps_python_exception_to_re() {
    const int uuid = 72004;
    remove_work_dir(uuid);

    PythonRunner runner;
    const SubmissionRequest request =
        make_request(uuid, "raise RuntimeError('boom')\n");

    const RunnerPrepareResult prepare_result = runner.prepare(request);
    const RunnerCompileResult compile_result = runner.compile(request);
    assert(prepare_result.ok);
    assert(compile_result.ok);

    const fs::path work_dir = make_work_dir(uuid);
    const fs::path input_path = work_dir / "case.in";
    const fs::path expected_path = work_dir / "case.out";
    write_file(input_path, "ignored\n");
    write_file(expected_path, "ignored\n");

    RunnerCaseInput input{};
    input.seq_id = 2;
    input.input_path = input_path.string();
    input.expected_output_path = expected_path.string();

    const RunnerCaseResult case_result = runner.runCase(request, input);

    assert(case_result.result.seq_id == 2);
    assert(case_result.result.verdict == SubmissionVerdict::RE);
    assert(case_result.result.exit_code != 0);

    remove_work_dir(uuid);
}

void test_run_case_maps_busy_loop_to_tle() {
    const int uuid = 72005;
    remove_work_dir(uuid);

    PythonRunner runner;
    const SubmissionRequest request =
        make_request(uuid, "while True:\n    pass\n");

    const RunnerPrepareResult prepare_result = runner.prepare(request);
    const RunnerCompileResult compile_result = runner.compile(request);
    assert(prepare_result.ok);
    assert(compile_result.ok);

    const fs::path work_dir = make_work_dir(uuid);
    const fs::path input_path = work_dir / "case.in";
    const fs::path expected_path = work_dir / "case.out";
    write_file(input_path, "ignored\n");
    write_file(expected_path, "ignored\n");

    RunnerCaseInput input{};
    input.seq_id = 3;
    input.input_path = input_path.string();
    input.expected_output_path = expected_path.string();
    input.real_time_limit_ms = 50;

    const RunnerCaseResult case_result = runner.runCase(request, input);

    assert(case_result.result.seq_id == 3);
    assert(case_result.result.verdict == SubmissionVerdict::TLE);

    remove_work_dir(uuid);
}

} // namespace

int main() {
    test_compile_check_succeeds_for_syntax_valid_python();
    test_compile_check_fails_for_syntax_invalid_python();
    test_run_case_returns_ac_for_matching_output();
    test_run_case_maps_python_exception_to_re();
    test_run_case_maps_busy_loop_to_tle();
    return 0;
}
