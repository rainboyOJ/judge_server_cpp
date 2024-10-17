#include "utils.h"

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
