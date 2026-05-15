/**
 * @file SubmissionService.cpp
 * @brief 异步评测编排服务实现。
 */

#include "pipeline/SubmissionService.h"

#include <filesystem>
#include <system_error>

#include "common/Config.h"
#include "common/Logger.h"
#include "common/utils.h"
#include "pipeline/SubmissionVerdictReducer.h"
#include "pipeline/ResultStore.h"
#include "runner/ILanguageRunner.h"
#include "runner/RunnerFactory.h"

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kLogFieldMaxLen = 80;

std::string sanitize_for_log(const std::string &value) {
  std::string sanitized;
  sanitized.reserve(value.size());
  for (unsigned char ch : value) {
    if (std::isalnum(ch) || ch == '_' || ch == '-' || ch == '.' || ch == ':') {
      sanitized.push_back(static_cast<char>(ch));
    } else {
      sanitized.push_back('_');
    }
  }

  if (sanitized.size() <= kLogFieldMaxLen) {
    return sanitized;
  }

  return sanitized.substr(0, kLogFieldMaxLen) + "...";
}

void log_failure_detail(const char *event, int submission_id,
                        const std::string &detail) {
  if (detail.empty()) {
    return;
  }

  LOG_DEBUG("%s submission_id=%d detail=%s", event, submission_id,
            sanitize_for_log(detail).c_str());
}

/** @brief 构造某个阶段的 SubmissionResult 快照。 */
SubmissionResult make_result(int submission_id, SubmissionStatus status,
                             SubmissionVerdict verdict,
                             const std::string &message = {}) {
  SubmissionResult result{};
  result.submission_id = submission_id;
  result.status = status;
  result.verdict = verdict;
  result.message = message;
  return result;
}

/** @brief 将快照持久化到 ResultStore。 */
bool persist_result(ResultStore &store, int submission_id,
                    const SubmissionResult &result) {
  return store.updateResult(submission_id, result);
}

bool persist_result_logged(ResultStore &store, int submission_id,
                           const SubmissionResult &result,
                           const std::string &log_pid, const char *transition) {
  if (persist_result(store, submission_id, result)) {
    return true;
  }

  LOG_ERROR("submission persist failed submission_id=%d pid=%s transition=%s "
            "status=%d verdict=%d",
            submission_id, log_pid.c_str(), transition,
            static_cast<int>(result.status), static_cast<int>(result.verdict));
  return false;
}

/** @brief 解析测试数据根目录。 */
fs::path resolve_test_data_root() {
  const fs::path project_root_data = fs::path(PROJECT_ROOT_DIR) / "testData";
  if (fs::exists(project_root_data)) {
    return project_root_data;
  }

  for (const fs::path &candidate :
       {fs::path("testData"), fs::path("../testData"),
        fs::path("../../testData")}) {
    if (fs::exists(candidate)) {
      return candidate;
    }
  }

  try {
    const fs::path configured_path = Config::getInstance().getTestDataPath();
    if (!configured_path.empty() && fs::exists(configured_path)) {
      return configured_path;
    }
  } catch (const std::exception &) {
    // 中文注释：配置未加载时直接回退到仓库内置路径，不让配置异常中断评测主流程。
  }

  return {};
}

/**
 * @brief SubmissionService 内部使用的测试点描述。
 */
struct SubmissionCaseSpec {
  RunnerCaseInput input;
};

/** @brief 根据 pid 从 testData 目录扫描出当前题目的 case 列表。 */
std::vector<SubmissionCaseSpec> load_cases_for_problem(const std::string &pid) {
  std::vector<SubmissionCaseSpec> cases;
  const fs::path test_data_root = resolve_test_data_root();
  if (test_data_root.empty()) {
    return cases;
  }

  const fs::path data_path = test_data_root / pid / "data";
  if (!fs::exists(data_path)) {
    return cases;
  }

  const TestDataFilePairs file_pairs = scan_test_data_file_pairs(data_path);
  int seq_id = 1;
  for (const auto &pair : file_pairs) {
    SubmissionCaseSpec spec;
    spec.input.seq_id = seq_id++;
    spec.input.input_path = (data_path / pair.first).string();
    spec.input.expected_output_path = (data_path / pair.second).string();
    spec.input.cpu_time_limit_ms = 1000;
    spec.input.real_time_limit_ms = 1000;
    spec.input.memory_limit_kb = 1024 * 1024;
    cases.push_back(spec);
  }

  return cases;
}

} // namespace

/** @copydoc SubmissionService::SubmissionService */
SubmissionService::SubmissionService(ResultStore &result_store,
                                     RunnerFactory &runner_factory,
                                     SubmissionVerdictReducer &judge_verdict_reducer)
    : result_store_(result_store), runner_factory_(runner_factory),
      judge_verdict_reducer_(judge_verdict_reducer) {}

/** @copydoc SubmissionService::createSubmission */
int SubmissionService::createSubmission(const SubmissionRequest &request) {
  return result_store_.createSubmission(request);
}

int SubmissionService::submitAsync(const SubmissionRequest &request,
                                   const std::string &reply_channel_id) {
  const int submission_id = createSubmission(request);
  if (submission_id <= 0) {
    return -1;
  }

  SubmissionTask task{};
  task.submission_id = submission_id;
  task.request = request;
  task.reply_channel_id = reply_channel_id;

  // 这里就是把任务正式移交给后台调度层的时刻：
  // 网络线程只负责 push 进 SubmissionQueue，真正的消费方是
  // JudgeWorkerPool::workerLoop() 中的 service_.waitTask()/queue_.pop()。
  if (!queue_.push(task)) {
    return -1;
  }

  return submission_id;
}

bool SubmissionService::waitTask(SubmissionTask &task) {
  return queue_.pop(task);
}

void SubmissionService::shutdownQueue() { queue_.shutdown(); }

std::size_t SubmissionService::queuedTaskCount() const { return queue_.size(); }

/** @copydoc SubmissionService::processSubmission */
void SubmissionService::processSubmission(int submission_id,
                                          const SubmissionRequest &request) {
  if (submission_id <= 0) {
    return;
  }

  const std::string log_pid = sanitize_for_log(request.pid);

  try {
    // 中文注释：先把提交推进到
    // PREPARING，后续任何系统级异常都基于这个提交单据回写。
    LOG_DEBUG("submission prepare enter submission_id=%d pid=%s", submission_id,
              log_pid.c_str());
    if (!persist_result_logged(result_store_, submission_id,
                               make_result(submission_id,
                                           SubmissionStatus::PREPARING,
                                           SubmissionVerdict::PENDING),
                               log_pid, "preparing")) {
      return;
    }

    const std::shared_ptr<ILanguageRunner> runner =
        runner_factory_.createRunner(request.language);
    if (runner == nullptr) {
      LOG_ERROR("submission runner missing submission_id=%d pid=%s "
                "reason=unsupported_language",
                submission_id, log_pid.c_str());
      persist_result_logged(result_store_, submission_id,
                            make_result(submission_id, SubmissionStatus::FAILED,
                                        SubmissionVerdict::SYSTEM_ERROR,
                                        "unsupported language"),
                            log_pid, "runner_missing");
      return;
    }

    SubmissionRequest execution_request = request;
    // 中文注释：执行阶段统一改用服务端生成的 submission_id 作为工作目录键，
    // 避免多个客户端复用同一个 uuid 时互相覆盖编译产物。
    execution_request.uuid = submission_id;

    const RunnerPrepareResult prepare_result =
        runner->prepare(execution_request);
    if (!prepare_result.ok) {
      LOG_ERROR("submission prepare failed submission_id=%d pid=%s "
                "reason=prepare_failed",
                submission_id, log_pid.c_str());
      log_failure_detail("submission prepare failed", submission_id,
                         prepare_result.message);
      persist_result_logged(result_store_, submission_id,
                            make_result(submission_id, SubmissionStatus::FAILED,
                                        SubmissionVerdict::SYSTEM_ERROR,
                                        prepare_result.message),
                            log_pid, "prepare_failed");
      return;
    }

    // 中文注释：prepare 成功后进入 COMPILING；解释型语言也统一走 compile
    // 阶段，保持状态机一致。
    if (!persist_result_logged(result_store_, submission_id,
                               make_result(submission_id,
                                           SubmissionStatus::COMPILING,
                                           SubmissionVerdict::PENDING),
                               log_pid, "compiling")) {
      return;
    }

    const RunnerCompileResult compile_result =
        runner->compile(execution_request);
    if (!compile_result.ok) {
      LOG_ERROR("submission compile failed submission_id=%d pid=%s verdict=%d "
                "reason=compile_failed",
                submission_id, log_pid.c_str(),
                static_cast<int>(compile_result.verdict));
      log_failure_detail("submission compile failed", submission_id,
                         compile_result.message);
      persist_result_logged(
          result_store_, submission_id,
          make_result(submission_id, SubmissionStatus::FINISHED,
                      compile_result.verdict, compile_result.message),
          log_pid, "compile_failed");
      return;
    }

    if (!persist_result_logged(result_store_, submission_id,
                               make_result(submission_id,
                                           SubmissionStatus::RUNNING,
                                           SubmissionVerdict::PENDING),
                               log_pid, "running")) {
      return;
    }

    const std::vector<SubmissionCaseSpec> cases =
        load_cases_for_problem(request.pid);
    if (cases.empty()) {
      LOG_ERROR("submission cases missing submission_id=%d pid=%s "
                "reason=problem_cases_missing",
                submission_id, log_pid.c_str());
      persist_result_logged(result_store_, submission_id,
                            make_result(submission_id, SubmissionStatus::FAILED,
                                        SubmissionVerdict::SYSTEM_ERROR,
                                        "failed to load problem cases"),
                            log_pid, "problem_cases_missing");
      return;
    }

    SubmissionResult running_result = make_result(
        submission_id, SubmissionStatus::RUNNING, SubmissionVerdict::PENDING);

    for (const SubmissionCaseSpec &case_spec : cases) {
      const RunnerCaseResult case_result =
          runner->runCase(execution_request, case_spec.input);
      running_result.case_results.push_back(case_result.result);
      running_result.message = case_result.message;
      if (!persist_result_logged(result_store_, submission_id, running_result,
                                 log_pid, "running_case_update")) {
        return;
      }
    }

    SubmissionResult finished_result =
        make_result(submission_id, SubmissionStatus::FINISHED,
                    judge_verdict_reducer_.summarize(running_result.case_results),
                    running_result.message);
    finished_result.case_results = running_result.case_results;
    if (!persist_result_logged(result_store_, submission_id, finished_result,
                               log_pid, "finished")) {
      return;
    }
    LOG_INFO(
        "submission verdict final submission_id=%d pid=%s status=%d verdict=%d",
        submission_id, log_pid.c_str(),
        static_cast<int>(finished_result.status),
        static_cast<int>(finished_result.verdict));
  } catch (const std::exception &ex) {
    LOG_ERROR("submission terminal exception submission_id=%d pid=%s "
              "reason=terminal_exception",
              submission_id, log_pid.c_str());
    log_failure_detail("submission terminal exception", submission_id,
                       ex.what());
    persist_result_logged(result_store_, submission_id,
                          make_result(submission_id, SubmissionStatus::FAILED,
                                      SubmissionVerdict::SYSTEM_ERROR,
                                      ex.what()),
                          log_pid, "terminal_exception");
  }
}

/** @copydoc SubmissionService::submit */
int SubmissionService::submit(const SubmissionRequest &request) {
  const int submission_id = createSubmission(request);
  processSubmission(submission_id, request);
  return submission_id;
}

/** @copydoc SubmissionService::query */
bool SubmissionService::query(int submission_id,
                              SubmissionResult &result) const {
  return result_store_.getResult(submission_id, result);
}
