#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "runner/CppRunner.h"

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
    request.language = SubmissionLanguage::CPP;
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

void test_compile_succeeds_for_tiny_valid_program() {
    const int uuid = 71001;
    remove_work_dir(uuid);

    CppRunner runner;
    const SubmissionRequest request = make_request(
        uuid,
        "#include <iostream>\nint main(){std::cout<<\"OK\\n\";return 0;}\n");

    const RunnerPrepareResult prepare_result = runner.prepare(request);
    const RunnerCompileResult compile_result = runner.compile(request);

    assert(prepare_result.ok);
    assert(compile_result.ok);
    assert(compile_result.verdict == SubmissionVerdict::AC);
    assert(fs::exists(make_work_dir(uuid) / "solution"));

    remove_work_dir(uuid);
}

void test_compile_fails_for_invalid_cpp_code() {
    const int uuid = 71002;
    remove_work_dir(uuid);

    CppRunner runner;
    const SubmissionRequest request =
        make_request(uuid, "int main( { return 0; }\n");

    const RunnerPrepareResult prepare_result = runner.prepare(request);
    const RunnerCompileResult compile_result = runner.compile(request);

    assert(prepare_result.ok);
    assert(!compile_result.ok);
    assert(compile_result.verdict == SubmissionVerdict::CE);
    assert(!compile_result.message.empty());
    assert(!fs::exists(make_work_dir(uuid) / "solution"));

    remove_work_dir(uuid);
}

void test_run_case_returns_ac_for_matching_output() {
    const int uuid = 71003;
    remove_work_dir(uuid);

    CppRunner runner;
    const SubmissionRequest request = make_request(
        uuid, "#include <iostream>\n#include <string>\nint main(){std::string "
              "s;std::getline(std::cin,s);std::cout<<s<<\"\\n\";return 0;}\n");

    const RunnerPrepareResult prepare_result = runner.prepare(request);
    const RunnerCompileResult compile_result = runner.compile(request);
    assert(prepare_result.ok);
    assert(compile_result.ok);

    const fs::path work_dir = make_work_dir(uuid);
    const fs::path input_path = work_dir / "case.in";
    const fs::path expected_path = work_dir / "case.out";
    write_file(input_path, "hello runner\n");
    write_file(expected_path, "hello runner\n");

    RunnerCaseInput input{};
    input.seq_id = 1;
    input.input_path = input_path.string();
    input.expected_output_path = expected_path.string();

    const RunnerCaseResult case_result = runner.runCase(request, input);

    assert(case_result.result.seq_id == 1);
    assert(case_result.result.verdict == SubmissionVerdict::AC);
    assert(case_result.result.exit_code == 0);
    assert(read_file(work_dir / "case_1.user.out") == "hello runner\n");

    remove_work_dir(uuid);
}

void test_run_case_returns_re_for_non_zero_exit_code() {
    const int uuid = 71004;
    remove_work_dir(uuid);

    CppRunner runner;
    const SubmissionRequest request =
        make_request(uuid, "#include <cstdlib>\nint main(){return 3;}\n");

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
    assert(case_result.result.exit_code == 3);

    remove_work_dir(uuid);
}

} // namespace

int main() {
    test_compile_succeeds_for_tiny_valid_program();
    test_compile_fails_for_invalid_cpp_code();
    test_run_case_returns_ac_for_matching_output();
    test_run_case_returns_re_for_non_zero_exit_code();
    return 0;
}
