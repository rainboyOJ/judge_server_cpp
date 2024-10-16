#include "testPointBox.h"
#include <iostream>


testPointBox::testPointBox(int poolSize) :stop(false)
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

void testPointBox::push(valPtr val)
{
    {
        std::unique_lock lck(work_mtx);
        que_.push(std::move(val));
    }
    cond_.notify_one();
}

void testPointBox::test(const testPoint * val) {
    std::cout << val->id_ <<  " " << val ->seq_id << std::endl;
}


// 具体的工作
void testPointBox::work() {
    while(1) {

        valPtr val;
        {
            std::unique_lock lck(work_mtx);
            std::cout << "before wait" << std::endl;
            while( !stop && que_.empty() )
            {
                cond_.wait(lck);
            }
            std::cout << "after wait" << std::endl;

            //检查是否结束
            // if( stop ) {
            if( stop && que_.empty() ) { // 数据为空时才会结束
                std::cout << "stop is true , thread exit" << std::endl;
                return;
            }

            // 取出数据
            val = std::move( que_.pop());
        }

        //检查是否为nullptr, 这里应该不可能为空!
        if( !val ) {
            std::cout << "val is nullptr" << std::endl;
            continue;
        }
        // todo do some real work;
        test(val.get());

    }

}
