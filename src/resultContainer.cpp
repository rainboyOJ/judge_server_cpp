#include "resultContainer.h"
#include "judgeInfo.h"

// TODO
void resultContainer::writeResult(int testBoxId,int seq_id,testPointResult * trp) {

    // std::lock_guard<std::mutex> lock(mtx_);
    // 对应testBoxId testResult 上锁
    std::lock_guard<std::mutex> lock(vec_[testBoxId].mtx_);
    auto & __Result__ = vec_[testBoxId];

    //遍历整个测试点，结果列表
    testPointResult * p = __Result__.trp;
    while(p) {
        if(p->seq_id == seq_id) {
            // 找到对应seq_id的测试点

            // 手动复制
            p->cpu_time = trp->cpu_time;
            p->real_time = trp->real_time;
            p->memory = trp->memory;
            p->signal = trp->signal;
            p->exit_code = trp->exit_code;
            p->error = trp->error;
            p->result = trp->result;

            ++__Result__.finish_cnt; // 完成计数加一
            return;
        }
        p = p->nxt;
    }
    
}

// TODO change return to std::vector<uint8_t> && ?? 要不要改呢?
// 编译器应该会自动优化的
std::vector<uint8_t> resultContainer::readResult(int idx, readResultStatus & status) {
    // std::lock_guard<std::mutex> lock(mtx_);

    // 对应testBoxId testResult 上锁
    std::lock_guard<std::mutex> lock(vec_[idx].mtx_);
    auto & __Result__ = vec_[idx];

    if( __Result__.finish_cnt == 0) {
        status = readResultStatus::NOT_DATA;
        return std::vector<uint8_t>();
    }


    if(__Result__.readDone_cnt <= __Result__.finish_cnt) {
        // 没有数据可以读取
        status = readResultStatus::NOT_NEW_DATA;
        return std::vector<uint8_t>();
    }

    // std::vector<uint8_t> result;
    __Result__.readDone_cnt = __Result__.finish_cnt;
    if( __Result__.finish_cnt == __Result__.data_size) {
        status = readResultStatus::FINISHED;
    }
    else {
        status = readResultStatus::SUCCESS;
    }

    return serializeTestPointResult(vec_[idx]);

    /*
        //得到testProblem的地址
        testProblem * tp = vec_[idx].test_problem_p.get();

        //1. uuid
        serializeInt(tp->uuid, result);

        //2. testError
        serializeInt(static_cast<int>(vec_[idx].err_type), result);

        // 3. msg TODO 要不要加入编译失败的信息呢?

        // 4. language 这里没有加
        // 因为不需要具体的language,本地肯定知道信息

        //3. test lang
        serializeInt(static_cast<int>(tp -> lang),result);
        return result;
    */
}