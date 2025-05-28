#include "resultContainer.h"
#include "judgeInfo.h"

void  testResult::clear()
{
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto &trp : TPR)
    {
        if (trp)
        {
            // 调用自定义删除器释放内存
            trp.reset();
        }
    }
    finish_cnt = 0;
    readDone_cnt = 0;
}

json testResult::serialize() const {
    json j;
    j["testBoxId"] = testBoxId;
    j["uuid"] = uuid;
    j["err_type"] = "succ";

    if( err_type == testError::prob_not_found) {
        j["err_type"] = "prob_not_found";
    }
    else if( err_type == testError::data_not_found) {
        j["err_type"] = "data_not_found";
    }
    else if( err_type == testError::compile_error) {
        j["err_type"] = "compile_error";
    }
    else if( err_type == testError::other) {
        j["err_type"] = "other";
    }

    j["data_size"] = data_size;
    j["finish_cnt"] = finish_cnt;
    j["readDone_cnt"] = readDone_cnt;

    if(test_problem_p) {
        j["test_problem"] = {
            {"uuid", test_problem_p->uuid},
            {"pid", std::string(test_problem_p->pid)},
            {"lang", static_cast<int>(test_problem_p->lang)},
            {"code", test_problem_p->code}
        };
    }

    // 序列化测试点结果
    testPointResult * current = trp;
    while (current) {
        j["results"].push_back({
            {"seq_id", current->seq_id},
            {"cpu_time", current->cpu_time},
            {"real_time", current->real_time},
            {"memory", current->memory},
            {"signal", current->signal},
            {"exit_code", current->exit_code},
            {"error", current->error},
            {"result", current->result}
        });
        current = current->nxt;
    }

    return j;
}