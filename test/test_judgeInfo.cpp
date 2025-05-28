// 测试judgeInfo 是否正确
#include <iostream>
#include "judgeInfo.h"
#include "utils.h"
#include "resultContainer.h"

using namespace std;

//创建一个测试 [提交评测]的评测
void test_Problem () {
    //创建一个评测题目的信息
    testProblem my_test;
    my_test.uuid = 9527;
    my_test.pid[0] = 'p';
    my_test.pid[1] = '1';
    my_test.pid[2] = '0';
    my_test.pid[3] = '0';
    my_test.pid[4] = '1';
    my_test.pid[5] = '\0';
    my_test.lang = language::cpp;
    my_test.code = "#include <iostream>";

    //序列化 
    std::vector<uint8_t> serialized_test = serializeTestProblem(my_test);
    //反序列化
    testProblem deserialized_my_test;
    deserializeTestProblem(serialized_test.data(), deserialized_my_test);
    // 输出

    cout << "uuid: " << deserialized_my_test.uuid << endl;
    cout << "pid: " << deserialized_my_test.pid << endl;
    cout << "lang: " << deserialized_my_test.lang << endl;
    cout << "code: " << deserialized_my_test.code << endl;
}

//测试结果的序列化与反序列化
void test_testResult () {

    // 创建一个
    // 创建一个测试结果
    testResult my_testResult;
    my_testResult.uuid = 9527;
    my_testResult.testBoxId = 1234;
    my_testResult.err_type = testError::compile_error;
    // my_testResult.lang = language::cpp;
    my_testResult.trp = nullptr;
    for (int i = 0; i < 9;i++)
    {
        testPointResult * t = new testPointResult;

        t->testBoxId = 1234;
        t->seq_id = i + 1;
        t->cpu_time = 1000 + i;
        t->real_time = 2000 + i;
        t->memory = 3000 + i;
        t->signal = 4000+i;
        t->exit_code = 5000+i;
        t->error = 6000+i;
        t->result = 7000+i;

        t->nxt = my_testResult.trp;
        my_testResult.trp = t;
    }

    //序列化
    std::vector<uint8_t> serialized_testResult = serializeTestPointResult(my_testResult);

    //反序列化
    testResultWithVecotr deserialized_my_testResult;
    deserializeTestPointResult(serialized_testResult.data(), deserialized_my_testResult);

    //输出反序列化后的结果
    cout << "uuid: " << deserialized_my_testResult.uuid << endl;
    cout << "testBoxId: " << deserialized_my_testResult.testBoxId << endl;
    cout << "err_type: " << deserialized_my_testResult.err_type << endl;
    cout << "lang: " << deserialized_my_testResult.lang << endl;
    cout << "[vector]: \n";
    for (auto &res : deserialized_my_testResult.trp)
    {
        cout << "seq_id: " << res.seq_id << endl;
        cout << "cpu_time: " << res.cpu_time << endl;
        cout << "real_time: " << res.real_time << endl;
        cout << "memory: " << res.memory << endl;
        cout << "signal: " << res.signal << endl;
        cout << "exit_code: " << res.exit_code << endl;
        cout << "error: " << res.error << endl;
        cout << "result: " << res.result << endl;
        cout << "=============\n\n";
    }

    // del memory
    while( my_testResult.trp != nullptr)
    {
        testPointResult *t = my_testResult.trp;
        my_testResult.trp = t->nxt;
        delete t;
    }
}

//测试JSON序列化功能
void test_JSON_serialization() {
    cout << "Testing JSON serialization functionality..." << endl;
    
    // 创建一个testResult对象
    testResult my_testResult;
    my_testResult.uuid = 9527;
    my_testResult.testBoxId = 1234;
    my_testResult.err_type = testError::succ;
    my_testResult.data_size = 3;
    my_testResult.finish_cnt = 3;
    my_testResult.readDone_cnt = 0;
    
    // 创建测试点结果链表
    my_testResult.trp = nullptr;
    for (int i = 0; i < 3; i++) {
        testPointResult * t = new testPointResult;
        t->testBoxId = 1234;
        t->seq_id = i + 1;
        t->cpu_time = 100 + i * 10;
        t->real_time = 200 + i * 20;
        t->memory = 1024 + i * 512;
        t->signal = 0;
        t->exit_code = 0;
        t->error = 0;
        t->result = 1; // AC
        
        t->nxt = my_testResult.trp;
        my_testResult.trp = t;
    }
    
    // 测试JSON序列化
    json j = my_testResult.serialize();
    cout << "JSON serialization result:" << endl;
    cout << j.dump(2) << endl;
    
    // 清理内存
    while(my_testResult.trp != nullptr) {
        testPointResult *t = my_testResult.trp;
        my_testResult.trp = t->nxt;
        delete t;
    }
    
    cout << "JSON serialization test completed!" << endl;
}

//测试resultContainer的JSON功能
void test_resultContainer_JSON() {
    cout << "Testing resultContainer JSON functionality..." << endl;
    
    // 创建resultContainer
    resultContainer container(5);
    
    // 初始化测试
    int testBoxId = 0;
    int data_size = 2;
    
    // 初始化测试箱
    auto trp = container.init_by_test_id(testBoxId, data_size);
    
    // 创建测试问题
    auto test_problem = std::make_unique<testProblem>();
    test_problem->uuid = 12345;
    strcpy(test_problem->pid, "P1001");
    test_problem->lang = language::cpp;
    test_problem->code = "#include <iostream>\nint main() { return 0; }";
    container.push_testProblem(testBoxId, std::move(test_problem));
    
    // 设置错误类型
    container.setErrorType(testBoxId, testError::succ);
    
    // 模拟测试点结果
    testPointResult* current = trp;
    for (int i = 0; i < data_size && current; i++) {
        current->cpu_time = 50 + i * 25;
        current->real_time = 100 + i * 50;
        current->memory = 2048 + i * 1024;
        current->signal = 0;
        current->exit_code = 0;
        current->error = 0;
        current->result = 1; // AC
        
        // 模拟完成
        bool finished = container.writeResult(testBoxId, i, current);
        cout << "Test point " << i << " finished. All tests done: " << (finished ? "true" : "false") << endl;
        
        current = current->nxt;
    }
    
    // 测试JSON读取
    readResultStatus status;
    json result = container.readResultAsJson(testBoxId, status);
    
    cout << "Read status: ";
    switch(status) {
        case readResultStatus::NOT_DATA:
            cout << "NOT_DATA"; break;
        case readResultStatus::NOT_NEW_DATA:
            cout << "NOT_NEW_DATA"; break;
        case readResultStatus::SUCCESS:
            cout << "SUCCESS"; break;
        case readResultStatus::FINISHED:
            cout << "FINISHED"; break;
    }
    cout << endl;
    
    cout << "JSON Result from resultContainer:" << endl;
    cout << result.dump(2) << endl;
    
    // 尝试再次读取（应该返回NOT_NEW_DATA）
    json result2 = container.readResultAsJson(testBoxId, status);
    cout << "Second read status: ";
    switch(status) {
        case readResultStatus::NOT_DATA:
            cout << "NOT_DATA"; break;
        case readResultStatus::NOT_NEW_DATA:
            cout << "NOT_NEW_DATA"; break;
        case readResultStatus::SUCCESS:
            cout << "SUCCESS"; break;
        case readResultStatus::FINISHED:
            cout << "FINISHED"; break;
    }
    cout << endl;
    
    cout << "resultContainer JSON test completed!" << endl;
}

int main()
{
    test_Problem();
    cout << "=============\n\n";
    test_testResult();
    test_JSON_serialization();
    test_resultContainer_JSON();
    return 0;
}
