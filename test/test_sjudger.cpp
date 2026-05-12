#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <unistd.h>

#include "sjudge_call.h"

namespace fs = std::filesystem;

namespace {

fs::path make_sjudger_dir(const std::string &name) {
    std::error_code ec;
    const fs::path temp_dir = fs::temp_directory_path(ec);
    assert(!ec);

    const auto unique_suffix =
        std::to_string(getpid()) + "_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path dir = temp_dir / (name + "_" + unique_suffix);

    ec.clear();
    fs::remove_all(dir, ec);
    assert(!ec);

    ec.clear();
    const bool created = fs::create_directories(dir, ec);
    assert(!ec);
    assert(created);
    return dir;
}

void test_invalid_config_returns_invalid_config_error() {
    judge_config config{};
    config.exe_path = "";
    config.max_cpu_time_ms = 10;

    const judge_result result = run_sjudger(config);

    assert(result.result == SYSTEM_ERROR || result.error == INVALID_CONFIG);
}

void test_missing_exe_is_invalid_config() {
    judge_config config{};
    config.exe_path.clear();

    const judge_result result = run_sjudger(config);

    assert(result.error == INVALID_CONFIG);
    assert(result.result == SYSTEM_ERROR);
}

void test_zero_limits_are_treated_as_unlimited_for_valid_exe() {
    judge_config config{};
    config.exe_path = "/bin/echo";
    config.args = {"/bin/echo", "ok"};
    config.max_cpu_time_ms = 0;
    config.max_real_time_ms = 0;
    config.max_memory_bytes = 0;
    config.max_stack_bytes = 0;
    config.max_output_bytes = 0;

    const judge_result result = run_sjudger(config);

    assert(result.result == SUCCESS);
    assert(result.error == 0);
}

void test_run_sjudger_executes_program_and_writes_output() {
    const fs::path dir = make_sjudger_dir("sjudger_exec_ok");
    const fs::path script = dir / "hello.sh";
    const fs::path input = dir / "in.txt";
    const fs::path output = dir / "out.txt";

    {
        std::ofstream stream(script);
        stream << "#!/bin/sh\nread line\nprintf '%s\\n' \"$line\"\n";
    }
    fs::permissions(script, fs::perms::owner_all, fs::perm_options::replace);
    {
        std::ofstream stream(input);
        stream << "hello\n";
    }

    judge_config config{};
    config.exe_path = script.string();
    config.args = {script.string()};
    config.input_path = input.string();
    config.output_path = output.string();
    config.cwd = dir.string();

    const judge_result result = run_sjudger(config);

    std::ifstream output_stream(output);
    std::string text;
    std::getline(output_stream, text);
    assert(result.result == SUCCESS);
    assert(result.exit_code == 0);
    assert(text == "hello");
}

} // namespace

int main() {
    test_invalid_config_returns_invalid_config_error();
    test_missing_exe_is_invalid_config();
    test_zero_limits_are_treated_as_unlimited_for_valid_exe();
    test_run_sjudger_executes_program_and_writes_output();
    return 0;
}
