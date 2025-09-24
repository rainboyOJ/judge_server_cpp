#include "resultContainer.h"
#include "judgeInfo.h"
#include <algorithm>
#include <stdexcept>

// ============= testSession 类实现 =============

testSession::testSession()
    : testBoxId(-1),
      uuid(-1),
      err_type(testError::succ),
      data_size(0),
      finish_cnt(0),
      readDone_cnt(0),
      TCR(100) // 初始化为支持最多100个测试用例
{
    // 构造函数：初始化所有成员为默认状态
}

bool testSession::writeResult(int idx, const TestCaseResult& tcr) {
    if (idx < 0 || idx >= static_cast<int>(TCR.size())) {
        return false; // 索引越界
    }
    
    std::lock_guard<std::mutex> lock(mtx_);
    
    // 检查这个位置是否已经有结果了
    if (TCR[idx].result == enum_testStatus::INIT) {
        // 只有在之前没有结果时才增加计数
        finish_cnt++;
    }
    
    // 写入结果
    TCR[idx] = tcr;
    
    // 返回是否所有测试用例都已完成
    return finish_cnt >= data_size;
}

json testSession::serialize() const {
    std::lock_guard<std::mutex> lock(mtx_);
    
    json j;
    j["testBoxId"] = testBoxId;
    j["uuid"] = uuid;
    
    // 序列化错误类型
    switch (err_type) {
        case testError::succ:
            j["err_type"] = "success";
            break;
        case testError::prob_not_found:
            j["err_type"] = "problem_not_found";
            break;
        case testError::data_not_found:
            j["err_type"] = "data_not_found";
            break;
        case testError::compile_error:
            j["err_type"] = "compile_error";
            break;
        case testError::other:
            j["err_type"] = "other_error";
            break;
        default:
            j["err_type"] = "unknown";
            break;
    }

    // 基本统计信息
    j["data_size"] = data_size;
    j["finish_cnt"] = finish_cnt;
    j["readDone_cnt"] = readDone_cnt;
    
    // 直接计算进度和完成状态，避免重复加锁
    double progress = (data_size > 0) ? static_cast<double>(finish_cnt) / static_cast<double>(data_size) : 0.0;
    bool is_finished = (finish_cnt >= data_size && data_size > 0);
    
    j["progress"] = progress;
    j["is_finished"] = is_finished;

    // 序列化题目信息
    if (test_problem_p) {
        j["test_problem"] = {
            {"uuid", test_problem_p->uuid},
            {"pid", std::string(test_problem_p->pid)},
            {"lang", static_cast<int>(test_problem_p->lang)},
            {"code", test_problem_p->code}
        };
    }

    // 序列化测试用例结果
    j["results"] = json::array();
    for (int i = 0; i < data_size && i < static_cast<int>(TCR.size()); ++i) {
        const auto& tcr = TCR[i];
        if (tcr.result != enum_testStatus::INIT) {
            j["results"].push_back({
                {"testBoxId", tcr.testBoxId},
                {"seq_id", tcr.seq_id},
                {"cpu_time", tcr.cpu_time},
                {"real_time", tcr.real_time},
                {"memory", tcr.memory},
                {"signal", tcr.signal},
                {"exit_code", tcr.exit_code},
                {"error", tcr.error},
                {"result", static_cast<int>(tcr.result)}
            });
        }
    }

    return j;
}

void testSession::clear() {
    std::lock_guard<std::mutex> lock(mtx_);
    
    // 重置所有测试用例结果为初始状态
    for (auto& tcr : TCR) {
        tcr.result = enum_testStatus::INIT;
        tcr.testBoxId = -1;
        tcr.seq_id = -1;
        tcr.cpu_time = 0;
        tcr.real_time = 0;
        tcr.memory = 0;
        tcr.signal = 0;
        tcr.exit_code = 0;
        tcr.error = 0;
        tcr.nxt = nullptr;
    }
    
    // 重置计数器
    finish_cnt = 0;
    readDone_cnt = 0;
}

void testSession::init_by_test_id(int testBoxId, int data_size) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    this->testBoxId = testBoxId;
    this->data_size = data_size;
    this->finish_cnt = 0;
    this->readDone_cnt = 0;
    
    // 确保TCR数组足够大
    if (data_size > static_cast<int>(TCR.size())) {
        TCR.resize(data_size);
    }
    
    // 初始化所有测试用例结果
    for (int i = 0; i < data_size; ++i) {
        TCR[i].result = enum_testStatus::INIT;
        TCR[i].testBoxId = testBoxId;
        TCR[i].seq_id = i;
        TCR[i].cpu_time = 0;
        TCR[i].real_time = 0;
        TCR[i].memory = 0;
        TCR[i].signal = 0;
        TCR[i].exit_code = 0;
        TCR[i].error = 0;
        TCR[i].nxt = nullptr;
    }
}

bool testSession::isFinished() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return finish_cnt >= data_size && data_size > 0;
}

double testSession::getProgress() const {
    std::lock_guard<std::mutex> lock(mtx_);
    if (data_size <= 0) return 0.0;
    return static_cast<double>(finish_cnt) / static_cast<double>(data_size);
}

// ============= resultContainer 类实现 =============

resultContainer::resultContainer(int size) : mem_() {
    // 创建足够的测试会话
    sessions_ = std::vector<testSession>(size + 5);
    
    // 初始化每个会话的基本信息
    for (int i = 0; i < static_cast<int>(sessions_.size()); ++i) {
        sessions_[i].testBoxId = i;
    }
}

resultContainer::~resultContainer() {
    std::lock_guard<std::mutex> lck(mtx_);
    
    // 清理所有会话
    for (auto& session : sessions_) {
        session.clear();
    }
}

void resultContainer::init_by_test_id(int testId, int data_size) {
    if (testId < 0 || testId >= static_cast<int>(sessions_.size())) {
        throw std::out_of_range("testId out of range");
    }

    auto& session = sessions_[testId];
    session.init_by_test_id(testId, data_size);
}

bool resultContainer::isJudgeFinished(int testBoxId) const {
    if (testBoxId < 0 || testBoxId >= static_cast<int>(sessions_.size())) {
        return false;
    }
    
    return sessions_[testBoxId].isFinished();
}


bool resultContainer::writeCaseResult(int testBoxId, int caseIndex, const TestCaseResult& result) {
    if (testBoxId < 0 || testBoxId >= static_cast<int>(sessions_.size())) {
        return false;
    }
    
    auto& session = sessions_[testBoxId];
    return session.writeResult(caseIndex, result);
}

void resultContainer::init_testProblem(int testBoxId, std::unique_ptr<testProblem> test_problem) {
    if (testBoxId < 0 || testBoxId >= static_cast<int>(sessions_.size()) || !test_problem) {
        return;
    }

    std::lock_guard<std::mutex> lck(mtx_);
    auto& session = sessions_[testBoxId];
    
    {
        std::lock_guard<std::mutex> session_lock(session.mtx_);
        session.test_problem_p = std::move(test_problem);
        session.uuid = session.test_problem_p->uuid;
    }
}

void resultContainer::init_testProblem(int testBoxId, int uuid, char pid[32], language lang, std::string&& code) {
    if (testBoxId < 0 || testBoxId >= static_cast<int>(sessions_.size())) {
        return;
    }

    auto& session = sessions_[testBoxId];
    std::lock_guard<std::mutex> lock(session.mtx_);

    session.test_problem_p = std::make_unique<testProblem>();
    session.test_problem_p->uuid = uuid;
    std::copy(pid, pid + 32, session.test_problem_p->pid);
    session.test_problem_p->lang = lang;
    session.test_problem_p->code = std::move(code);
    session.uuid = uuid;
}

void resultContainer::setErrorType(int testBoxId, testError err_type) {
    if (testBoxId < 0 || testBoxId >= static_cast<int>(sessions_.size())) {
        return;
    }

    auto& session = sessions_[testBoxId];
    std::lock_guard<std::mutex> lock(session.mtx_);
    session.err_type = err_type;
}

json resultContainer::GetResultAsJson(int idx, readResultStatus& status) {
    if (idx < 0 || idx >= static_cast<int>(sessions_.size())) {
        status = readResultStatus::EXCEED_TESTBOX_ID;
        return json{};
    }

    auto& session = sessions_[idx];
    std::lock_guard<std::mutex> lock(session.mtx_);

    // 检查是否有数据
    if (session.finish_cnt == 0) {
        status = readResultStatus::NOT_DATA;
        return json{};
    }

    // 检查是否有新数据
    if (session.readDone_cnt >= session.finish_cnt) {
        status = readResultStatus::NOT_NEW_DATA;
        return json{};
    }

    // 更新已读计数
    session.readDone_cnt = session.finish_cnt;
    
    // 确定状态
    if (session.finish_cnt == session.data_size) {
        status = readResultStatus::FINISHED;
    } else {
        status = readResultStatus::SUCCESS;
    }

    return session.serialize();
}


void resultContainer::ResetTestSessionById(int idx) {
    if (idx < 0 || idx >= static_cast<int>(sessions_.size())) {
        return;
    }
    
    auto& session = sessions_[idx];
    session.clear();
}

TestCaseResult* resultContainer::AllocateTestCaseResult_of_N(int n) {
    if (n <= 0) return nullptr;
    
    std::lock_guard<std::mutex> lck(mtx_);
    TestCaseResult* head = nullptr;
    
    for (int i = 0; i < n; ++i) {
        TestCaseResult* result = mem_.get();
        if (!result) break; // 内存池耗尽
        
        result->nxt = head;
        head = result;
    }
    
    return head;
}