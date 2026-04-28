//临时测试的
#include <iostream>
#include <cstring>
#include "legacy/judgeInfo.h"
#include "utils.h"
using namespace std;

char str[] = "00 00 00 00 00 00 00 00 00 00 00 0a 00 00 00 01 00 00 00 02 00 00 00 00 00 00 00 00 00 00 06 34 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 02 00 00 00 01 00 00 00 00 00 00 00 00 00 00 06 b4 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 03 00 00 00 01 00 00 00 00 00 00 00 00 00 00 06 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 04 00 00 00 01 00 00 00 00 00 00 00 00 00 00 06 a0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 05 00 00 00 01 00 00 00 00 00 00 00 00 00 00 06 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 06 00 00 00 02 00 00 00 00 00 00 00 00 00 00 06 b0 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 07 00 00 00 01 00 00 00 00 00 00 00 00 00 00 06 a8 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 08 00 00 00 01 00 00 00 00 00 00 00 00 00 00 06 a8 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 09 00 00 00 01 00 00 00 00 00 00 00 00 00 00 06 88 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0a 00 00 00 01 00 00 00 00 00 00 00 00 00 00 06 08 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ";

std::vector<uint8_t> data_;

int main(int argc, char const *argv[])
{

    int size = strlen(str);
    string s;
    for (int i = 0; i < size;i++) {
        if( str[i] == ' ') {
            //转成16进制
            uint8_t a = stoi(s, nullptr, 16);
            // cout << std:: hex << (int)a << " ";
            s = "";
            data_.push_back(a);
        }
        s += str[i];
    }

    debug_print_uint8_t_vector(data_);

    testResultWithVecotr testResult;
    deserializeTestPointResult(data_.data(), testResult);

    std::cout << "Result info :\n";
    std::cout << "uuid: " << testResult.uuid << std::endl;
    std::cout << "testBoxId " << testResult.testBoxId << std::endl;
    std::cout << "err_type " << testResult.err_type << std::endl;
    std::cout << "lang " << testResult.lang<< std::endl;
    for(auto& point_result : testResult.trp)
    {
        std::cout << "seq_id: " << point_result.seq_id
                  << " cpu_time: " << point_result.cpu_time
                  << " real_time: " << point_result.real_time
                  << " memory: " << point_result.memory
                  << " signal: " << point_result.signal
                  << " exit_code: " << point_result.exit_code
                  << " error: " << point_result.error
                  << " result: " << point_result.result << std::endl;
    }

    return 0;
}
