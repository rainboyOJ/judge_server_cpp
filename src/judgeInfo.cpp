#include <iostream>
#include <iomanip>
#include "judgeInfo.h"
#include "Logger.h"

// ==================== 

std::vector<uint8_t> serializeTestProblem(const testProblem &tp){
    std::vector<uint8_t> ret; //开头填充4个字节，后面填充内容
    // 这4个字节用来存放后面所有长度值
    serializeInt(tp.uuid,ret);

    for(int i = 0 ;i < sizeof(tp.pid)/sizeof(tp.pid[0]) ;i++)
    {
        ret.push_back(tp.pid[i]);
    }

    serializeInt(static_cast<int>(tp.lang),ret);

    serializeInt<int>(tp.code.size(),ret); // 存放code长度

    for(int i = 0 ;i < tp.code.size() ;i++)
        ret.push_back(tp.code[i]);

    //记录长度
    // *(int *)&ret[0] = htonl(tot_len);

    return std::move(ret);
}

//反序列化testProblem
void deserializeTestProblem(const uint8_t * s, testProblem &tp) {
    // 解析长度,这里不用解析长度了,因为长度已经在前面序列化的时候记录了
    // int tot_len = deserializeInt<int>(s+idx);
    // idx += sizeof(int);

    int idx = 0; //下标
    // 解析uuid
    tp.uuid = deserializeInt<int>(s+idx);
    idx += sizeof( tp.uuid );
    LOG_DEBUG("uuid: %d \n", tp.uuid);

    // 解析pid
    for(int i = 0 ;i < sizeof(tp.pid)/sizeof(tp.pid[0]) ;i++)
    {
        tp.pid[i] = s[idx];
        idx += 1;
        // LOG_DEBUG("pid[%d]  = %c \n", i,tp.pid[i]);
    }

    // 解析lang
    tp.lang = static_cast<language>(deserializeInt<int>(s+idx));
    idx += sizeof(int);

    // 解析code长度
    int code_len = deserializeInt<int>(s+idx);
    idx += sizeof(int);

    // 解析code
    for(int i = 0 ;i < code_len ;i++)
    {
        tp.code += s[idx];
        idx += 1;
    }
    LOG_DEBUG("test_problem code \n%s\n", tp.code.c_str());
}

std::vector<uint8_t> serializeTestPointResult(const testResult &tpr){
    std::vector<uint8_t> ret;
    //注意这里的结果有多种可能性
    // [信息头] + [testPointResultArray] 组成
    // 1. uuid
    serializeInt(tpr.uuid,ret);

    // 这里没有解析testBoxId 因应没有用

    // 2. testError
    serializeInt(static_cast<int>(tpr.err_type),ret);

    // 3. msg TODO 要不要加入编译失败的信息呢?

    // 4. language 这里没有加
    // 因为不需要具体的language,本地肯定知道信息

    int testPointResultArray_len = 0;
    // 得到结果的链表的长度
    testPointResult * head = tpr.trp;
    while(head != nullptr ) {
        ++testPointResultArray_len;
        head = head->nxt;
    }

    serializeInt(testPointResultArray_len,ret);

    head = tpr.trp;
    while(head != nullptr ) {
        serializeInt(head->seq_id,ret);
        serializeInt(head->cpu_time,ret);
        serializeInt(head->real_time,ret);
        serializeInt<unsigned long long>(head->memory,ret);
        serializeInt(head->signal,ret);
        serializeInt(head->exit_code,ret);
        serializeInt(head->error,ret);
        serializeInt(head->result,ret);
        head = head->nxt;
    }

    return std::move(ret);
}