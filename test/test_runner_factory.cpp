#include <cassert>
#include <memory>

#include "runner/ILanguageRunner.h"
#include "runner/RunnerFactory.h"

namespace {

void test_cpp_language_returns_non_null_runner() {
    RunnerFactory factory;

    const std::shared_ptr<ILanguageRunner> runner =
        factory.createRunner(SubmissionLanguage::CPP);

    assert(runner != nullptr);
}

void test_python_language_returns_non_null_runner() {
    RunnerFactory factory;

    const std::shared_ptr<ILanguageRunner> runner =
        factory.createRunner(SubmissionLanguage::PYTHON);

    assert(runner != nullptr);
}

void test_unsupported_language_returns_null_runner() {
    RunnerFactory factory;

    const std::shared_ptr<ILanguageRunner> runner =
        factory.createRunner(SubmissionLanguage::C);

    assert(runner == nullptr);
}

void test_returned_runner_satisfies_language_runner_interface() {
    RunnerFactory factory;
    SubmissionRequest request{};
    request.language = SubmissionLanguage::CPP;

    const std::shared_ptr<ILanguageRunner> runner =
        factory.createRunner(SubmissionLanguage::CPP);

    assert(runner != nullptr);

    const RunnerPrepareResult prepare_result = runner->prepare(request);
    const RunnerCompileResult compile_result = runner->compile(request);
    const RunnerCaseResult case_result = runner->runCase(request, {});

    assert(!prepare_result.ok);
    assert(!compile_result.ok);
    assert(case_result.result.verdict == SubmissionVerdict::SYSTEM_ERROR);
}

} // namespace

int main() {
    test_cpp_language_returns_non_null_runner();
    test_python_language_returns_non_null_runner();
    test_unsupported_language_returns_null_runner();
    test_returned_runner_satisfies_language_runner_interface();
    return 0;
}
