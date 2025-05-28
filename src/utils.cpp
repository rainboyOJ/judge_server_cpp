#include "utils.h"
#include "Logger.h"

std::string Popen(const char* cmd) {
    // 使用popen执行命令并读取输出
    std::array<char, 128> buffer;  // 用于存储输出
    std::string result;              // 存储最终结果

    // 打开一个管道执行命令
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }

    // 逐行读取输出
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;  // 返回命令的输出结果
}

		// --max-cpu-time $3 \
		// --max-real-time $(expr $3 + 1) \
		// --max-memory $2 \
		// --exe $1 \
		// --input $4 \
		// --output /tmp/sjudge.out \
		// --error err.txt \
		// --args ls \
		// --args -l \
		// --env foo=bar \
		// --log log.txt)
std::string judge(
    long long max_cpu_time,
    long long max_real_time,
    long long max_memory,
    std::string exe, //程序
    std::string input,
    std::string output
) {
    std::string judge_args = "/usr/bin/sjudge ";
    judge_args += " --max-cpu-time ";
    judge_args += std::to_string(max_cpu_time);
    judge_args += " --max-real-time ";
    judge_args += std::to_string(max_real_time);
    judge_args += " --max-memory ";
    judge_args += std::to_string(max_memory);

    judge_args += " --exe ";
    judge_args += exe;

    if( input.length()) {
        judge_args += " --input ";
        judge_args += input;
    }
    judge_args += " --output ";
    judge_args += output;
    return Popen(judge_args.c_str());
}

void parse_test_point_result(const std::string & str,testPointResult * result){
    enum class State {
        begin,
        name,
        val,
        end
    };

    State state = State::begin;
    std::string val;
    std::string name;

    auto isblank = [&str](int i) {
        char c ;
        if( i== -1 )
            c= '\n';
        else
            c = str[i];
        return ( c == '\n' || c== ' ' || c == '\r' );
    };

    for(int i = 0 ;i< str.length(); ++i) {
        if(isblank(i) && isblank(i-1))
            continue;

        char c = str[i];
        //不是空白，但是最后一个字符
        if( !isblank(i) ) {
            if( state == State::begin) {
                state = State::name;
                name+=c;
            }
            else if( state == State::name) 
                name+=c;
            else if( state == State::val) 
                val+=c;
            continue;
        }


        if(isblank(i) || i+1 == str.length()) {
            if( state == State::val) {
                // LOG_INFO("in parse_test_point_result :  name = %s,val = %s\n",name.c_str(),val.c_str());
                if( name == "result") {
                    result->result= std::stoi(val);
                }
                else if( name == "signal")
                {
                    result->signal = std::stoi(val);
                }
                else if( name == "memory")
                {
                    result->memory= std::stoll(val);

                }
                else if( name == "exit_code")
                {
                    result->exit_code = std::stoi(val);

                }
                else if( name == "error")
                {
                    result->error= std::stoi(val);

                }
                else if( name == "real_time")
                {
                    result->real_time= std::stoi(val);

                }
                else if( name == "cpu_time")
                {
                    result->cpu_time = std::stoi(val);

                }

                val.clear();
                name.clear();
                state = State::name;
            }
            else if( state == State::name)
            {

                state = State::val;
            }
        }
    }
}

//反序列化testPointResult
void deserializeTestPointResult(const uint8_t* s, testResultWithVecotr &tpr) {
    int idx = 0;
    //1. 解析uuid
    tpr.uuid = deserializeInt<decltype(tpr.uuid)>(s + idx);
    idx+=sizeof(tpr.uuid);

    // 2. 解析error_type
    tpr.err_type = static_cast<testError>(deserializeInt<int>(s + idx));
    idx+=sizeof(int);

    // 得到结果的链表的长度
    int testPointResultArray_len = deserializeInt<int>(s + idx);
    idx+= sizeof(int); //记录长度的是int类型

    for (int i = 0; i < testPointResultArray_len;i++)
    {
        testPointResult tp;

        tp.seq_id = deserializeInt<decltype(tp.seq_id)>(s + idx);
        idx+= sizeof(tp.seq_id);
        tp.cpu_time = deserializeInt<decltype(tp.cpu_time)>(s + idx);
        idx+= sizeof(tp.cpu_time);
        tp.real_time = deserializeInt<decltype(tp.real_time)>(s + idx);
        idx+= sizeof(tp.real_time);
        //这里是强指定的
        tp.memory = deserializeInt<unsigned long long>(s + idx);
        idx+= sizeof(tp.memory);
        tp.signal = deserializeInt<decltype(tp.signal)>(s + idx);
        idx+= sizeof(tp.signal);
        tp.exit_code = deserializeInt<decltype(tp.exit_code)>(s + idx);
        idx+= sizeof(tp.exit_code);
        tp.error = deserializeInt<decltype(tp.error)>(s + idx);
        idx+= sizeof(tp.error);
        tp.result = deserializeInt<decltype(tp.result)>(s + idx);
        idx+= sizeof(tp.result);

        tpr.trp.push_back(tp);
    }
}

void debug_print_uint8_t_vector(const std::vector<uint8_t>& vec) {
    int cnt = 0;
    for (uint8_t byte : vec)
    {
        std::cout << std::setw(2)      // 宽度为 2 位
                  << std::setfill('0') // 如果不足 2 位，填充零
                  << std::hex          // 十六进制输出格式
                  << (int)byte << " ";
        ++cnt;
        if( cnt == 16) 
        {
            std::cout << std::endl;
            cnt = 0;
        }
    }
    std::cout << std::endl;
}
