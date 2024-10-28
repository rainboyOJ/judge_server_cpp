#include <iostream>
#include "testPointBox.h"
#include "Logger.h"
#include "utils.h"
#include "testBox.h"



testPointBox::testPointBox(int poolSize, testBox * testBoxPtr)
:stop(false),
testBox_(testBoxPtr),
que_head(nullptr),
que_tail(nullptr)
{
    //创建线程工作函数
    for(int i = 0 ;i <poolSize ;++i)
    {
        workers_.emplace_back(&testPointBox::work,this);
    }
}

testPointBox::~testPointBox() {
    {
        std::unique_lock lck(work_mtx);
        stop = true;
    }
    cond_.notify_all();
    for( auto & th : workers_)
    {
        std::cout << "join " << "\n";
        th.join();
    }
}

void testPointBox::push_one(testPoint * tp)
{
    tp->nxt = nullptr;
    {
        std::unique_lock lck(work_mtx);
        push_que(tp);
    }
    cond_.notify_one();
}

void testPointBox::push_link(testPoint* tp){
    {
        std::lock_guard lck(work_mtx);
        push_que(tp);
    }
    cond_.notify_all();
}

// TODO 这里申请的内存,注意释放 ,应该在work线程里释放
testPoint *  testPointBox::get_testPoint_link(int size){
    testPoint *link_head = nullptr;
    std::lock_guard lck(memPool_mtx_);
    while(size--) {
        testPoint * tp = memPool_.get();
        tp->nxt = link_head;
        link_head = tp;
    }
    return link_head;
}

//核心评测
void testPointBox::test(const testPoint * val) {
    // 1. 根据信息来执行
    // LOG_INFO("id_ : %s , seq id: %d\n",val->id_,val->seq_id);
    // 进行真正的测试
    std::string resultStr = judge(1000,2000,12800,"/usr/bin/ls","",std::to_string(val->seq_id) + ".out");
    LOG_INFO("seq id: %d, result = %s\n",val->seq_id,resultStr.c_str());
    
    // 2.解析评测结果字符串
    auto resultPtr = val->testPointResult_p; //得到需要写入结果的地址
    resultPtr ->seq_id = val-> seq_id;
    resultPtr ->testBoxId = val-> testBoxId;
    parse_test_point_result(resultStr,resultPtr);

    if( val -> testPointResult_p != nullptr) {
        // TODO 写入结果到resultPtr 对应的地址
    }

    //解析
    // real_time 0
    // memory 1648
    // signal 0
    // exit_code 0
    // error 0
    // result 0
    // 调用上一层testBox函数
    if(testBox_ != nullptr)
        testBox_ -> deal_testPoint_singlePointComplete(resultPtr);

}


// 具体的工作
void testPointBox::work() {
    while(1) {

        testPoint * tp = nullptr;
        {
            std::unique_lock lck(work_mtx);
            while( !stop && que_empty() )
            {
                cond_.wait(lck);
            }

            //检查是否结束
            // if( stop ) {
            if( stop && que_empty() ) { // 数据为空时才会结束
                std::cout << "stop is true , thread exit" << std::endl;
                return;
            }

            // 取出数据
            // val = std::move( que_.pop());
            tp = pop_que();
        }

        //检查是否为nullptr, 这里应该不可能为空!
        if( !tp ) {
            // std::cout << "val is nullptr" << std::endl;
            continue;
        }
        // todo do some real work;
        test(tp);

    }

}
