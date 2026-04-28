#pragma once

#include <memory>

#include "common/SubmissionTypes.h"

class ILanguageRunner;

/**
 * @brief 根据提交语言创建对应 runner 的工厂。
 */
class RunnerFactory {
  public:
    /**
     * @brief 创建与给定语言匹配的 runner。
     * @return std::shared_ptr<ILanguageRunner> 若语言不支持则返回 nullptr。
     */
    std::shared_ptr<ILanguageRunner>
    createRunner(SubmissionLanguage language) const;
};
