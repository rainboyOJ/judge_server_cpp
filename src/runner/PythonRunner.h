#pragma once

#include "runner/ILanguageRunner.h"

/**
 * @brief Python 提交的执行器。
 */
class PythonRunner : public ILanguageRunner {
  public:
    /** @copydoc ILanguageRunner::prepare */
    RunnerPrepareResult prepare(const SubmissionRequest &request) override;
    /** @copydoc ILanguageRunner::compile */
    RunnerCompileResult compile(const SubmissionRequest &request) override;
    /** @copydoc ILanguageRunner::runCase */
    RunnerCaseResult runCase(const SubmissionRequest &request,
                             const RunnerCaseInput &test_case) override;
};
