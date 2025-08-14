#include "judgeInfo.h"
#include "workThreadPool.h"

// 编译阶段
bool Compile(const int testBoxId, resultContainer *resultContainerPtr) {
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