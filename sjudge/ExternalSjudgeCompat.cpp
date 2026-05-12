#include "ExternalSjudgeCompat.h"

#include <array>
#include <cstdio>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

struct PipeCloser {
  void operator()(FILE *pipe) const {
    if (pipe != nullptr) {
      pclose(pipe);
    }
  }
};

void append_limit_arg(std::string &command, const char *name, uint64_t value) {
  if (value != SJUDGE_UNLIMITED) {
    command += " ";
    command += name;
    command += " ";
    command += std::to_string(value);
  }
}

void append_path_arg(std::string &command, const char *name,
                     const std::string &value) {
  if (!value.empty()) {
    command += " ";
    command += name;
    command += " ";
    command += value;
  }
}

void parse_result_line(judge_result &result, const std::string &line) {
  const size_t pos = line.find_first_of("0123456789");
  if (pos == std::string::npos) {
    return;
  }

  if (line.find("cpu_time") == 0) {
    result.cpu_time_ms = std::stoi(line.substr(pos));
  } else if (line.find("real_time") == 0) {
    result.real_time_ms = std::stoi(line.substr(pos));
  } else if (line.find("memory") == 0) {
    result.memory_bytes = std::stoll(line.substr(pos));
  } else if (line.find("signal") == 0) {
    result.signal = std::stoi(line.substr(pos));
  } else if (line.find("exit_code") == 0) {
    result.exit_code = std::stoi(line.substr(pos));
  } else if (line.find("error") == 0) {
    result.error = std::stoi(line.substr(pos));
  } else if (line.find("result") == 0) {
    result.result = std::stoi(line.substr(pos));
  }
}

} // namespace

judge_result call_sjudge(const char *sjudge_binary_path,
                         const judge_config &config) {
  std::string command = sjudge_binary_path;

  append_limit_arg(command, "--max-cpu-time", config.max_cpu_time_ms);
  append_limit_arg(command, "--max-real-time", config.max_real_time_ms);
  append_limit_arg(command, "--max-memory", config.max_memory_bytes);
  append_limit_arg(command, "--max-stack", config.max_stack_bytes);
  append_limit_arg(command, "--max-output", config.max_output_bytes);
  append_path_arg(command, "--cwd", config.cwd);
  append_path_arg(command, "--exe", config.exe_path);
  append_path_arg(command, "--input", config.input_path);
  append_path_arg(command, "--output", config.output_path);
  append_path_arg(command, "--error", config.error_path);
  append_path_arg(command, "--log", config.log_path);
  command += " 2>&1";

  std::array<char, 128> buffer{};
  std::string output;
  std::unique_ptr<FILE, PipeCloser> pipe(popen(command.c_str(), "r"));
  if (!pipe) {
    throw std::runtime_error("popen() failed");
  }

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    output += buffer.data();
  }

  judge_result result{};
  std::istringstream stream(output);
  std::string line;
  while (std::getline(stream, line)) {
    parse_result_line(result, line);
  }
  return result;
}
