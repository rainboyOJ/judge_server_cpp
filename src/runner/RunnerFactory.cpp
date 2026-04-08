#include "runner/RunnerFactory.h"

#include <memory>

#include "runner/CppRunner.h"
#include "runner/PythonRunner.h"

std::shared_ptr<ILanguageRunner>
RunnerFactory::createRunner(SubmissionLanguage language) const {
    switch (language) {
    case SubmissionLanguage::CPP:
        return std::make_shared<CppRunner>();
    case SubmissionLanguage::PYTHON:
        return std::make_shared<PythonRunner>();
    default:
        break;
    }

    return nullptr;
}
