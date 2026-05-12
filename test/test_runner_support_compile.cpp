#include "runner/RunnerSupport.h"
#include "sjudge_call.h"

namespace fs = std::filesystem;

namespace {

using CompileCppSourceFileFn = bool (*)(const fs::path &source_file,
                                        const fs::path &executable_file,
                                        const fs::path &compile_log_file,
                                        std::string &compile_output);

using CompareCaseOutputFn = SubmissionVerdict (*)(
    const fs::path &input_path,
    const fs::path &expected_output,
    const fs::path &user_output);

using RunExecutableCaseFn = RunnerCaseResult (*)(
    const fs::path &executable_path,
    const RunnerCaseInput &test_case,
    const fs::path &user_output_path);

using RunSjudgerFn = judge_result (*)(const judge_config &config);

void test_runner_support_header_exposes_expected_helpers() {
    const CompileCppSourceFileFn compile_fn = &compile_cpp_source_file;
    const CompareCaseOutputFn compare_fn = &compare_case_output;
    const RunExecutableCaseFn run_fn = &run_executable_case;
    const RunSjudgerFn sjudger_fn = &run_sjudger;

    (void)compile_fn;
    (void)compare_fn;
    (void)run_fn;
    (void)sjudger_fn;
}

} // namespace

int main() {
    test_runner_support_header_exposes_expected_helpers();
    return 0;
}
