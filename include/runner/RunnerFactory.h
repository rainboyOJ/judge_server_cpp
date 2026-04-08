#pragma once

#include <memory>

#include "common/SubmissionTypes.h"

class ILanguageRunner;

class RunnerFactory {
  public:
    // 中文注释：MVP
    // 阶段先固定语言到实现类的映射，后续任务再补充真实编译运行逻辑。
    std::shared_ptr<ILanguageRunner>
    createRunner(SubmissionLanguage language) const;
};
