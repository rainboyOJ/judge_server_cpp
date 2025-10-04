#include "judgeInfo.h"
#include "workThreadPool.h"
#include "common/Logger.h"

// 编译阶段
bool Compile(const int testBoxId, resultContainer *resultContainerPtr) {
    LOG_DEBUG("Compile testBoxId %d", testBoxId);
    language lang = resultContainerPtr->getLanguage(testBoxId);
    if (lang == language::cpp ){
        // compile
    }
    else { // 
        
        // 向resulContainer 写入错误信息: do not support language
        return false;
    }
    return true;
}