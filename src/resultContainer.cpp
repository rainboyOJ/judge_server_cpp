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
    for( const auto & t : TPR) {
        if( t) {
            j["results"].push_back({
                {"testBoxId", t->testBoxId},
                {"seq_id", t->seq_id},
                {"cpu_time", t->cpu_time},
                {"real_time", t->real_time},
                {"memory", t->memory},
                {"signal", t->signal},
                {"exit_code", t->exit_code},
                {"error", t->error},
                {"result", t->result}
            });
        }
    }
    return j;
}