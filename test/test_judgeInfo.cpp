// 测试judgeInfo 是否正确
#include <iostream>
#include "judgeInfo.h"
#include "utils.h"

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

int main()
{
    test_Problem();
    cout << "=============\n\n";
    test_testResult();
    return 0;
}
