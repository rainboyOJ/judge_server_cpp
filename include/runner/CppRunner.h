#pragma once

#include "runner/ILanguageRunner.h"

class CppRunner : public ILanguageRunner {
  public:
    RunnerPrepareResult prepare(const SubmissionRequest &request) override;
    RunnerCompileResult compile(const SubmissionRequest &request) override;
    RunnerCaseResult runCase(const SubmissionRequest &request,
                             const RunnerCaseInput &test_case) override;
};
