#include "service/SubmissionService.h"

#include <filesystem>
#include <system_error>

#include "common/Config.h"
#include "judge/JudgeCore.h"
#include "runner/ILanguageRunner.h"
#include "runner/RunnerFactory.h"
#include "store/ResultStore.h"
#include "utils.h"

namespace fs = std::filesystem;

namespace {

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

bool persist_result(ResultStore &store, int submission_id,
                    const SubmissionResult &result) {
    return store.updateResult(submission_id, result);
}

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
        const fs::path configured_path =
            Config::getInstance().getTestDataPath();
        if (!configured_path.empty() && fs::exists(configured_path)) {
            return configured_path;
        }
    } catch (const std::exception &) {
        // 中文注释：配置未加载时直接回退到仓库内置路径，不让配置异常中断评测主流程。
    }

    return {};
}

struct SubmissionCaseSpec {
    RunnerCaseInput input;
};

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

    const Data_list_t file_pairs = scan_data_list(data_path);
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

SubmissionService::SubmissionService(ResultStore &result_store,
                                     RunnerFactory &runner_factory,
                                     JudgeCore &judge_core)
    : result_store_(result_store), runner_factory_(runner_factory),
      judge_core_(judge_core) {}

int SubmissionService::submit(const SubmissionRequest &request) {
    const int submission_id = result_store_.createSubmission(request);

    try {
        // 中文注释：先把提交推进到
        // PREPARING，后续任何系统级异常都基于这个提交单据回写。
        if (!persist_result(result_store_, submission_id,
                            make_result(submission_id,
                                        SubmissionStatus::PREPARING,
                                        SubmissionVerdict::PENDING))) {
            return submission_id;
        }

        const std::shared_ptr<ILanguageRunner> runner =
            runner_factory_.createRunner(request.language);
        if (runner == nullptr) {
            persist_result(result_store_, submission_id,
                           make_result(submission_id, SubmissionStatus::FAILED,
                                       SubmissionVerdict::SYSTEM_ERROR,
                                       "unsupported language"));
            return submission_id;
        }

        SubmissionRequest execution_request = request;
        // 中文注释：执行阶段统一改用服务端生成的 submission_id 作为工作目录键，
        // 避免多个客户端复用同一个 uuid 时互相覆盖编译产物。
        execution_request.uuid = submission_id;

        const RunnerPrepareResult prepare_result =
            runner->prepare(execution_request);
        if (!prepare_result.ok) {
            persist_result(result_store_, submission_id,
                           make_result(submission_id, SubmissionStatus::FAILED,
                                       SubmissionVerdict::SYSTEM_ERROR,
                                       prepare_result.message));
            return submission_id;
        }

        // 中文注释：prepare 成功后进入 COMPILING；解释型语言也统一走 compile
        // 阶段，保持状态机一致。
        if (!persist_result(result_store_, submission_id,
                            make_result(submission_id,
                                        SubmissionStatus::COMPILING,
                                        SubmissionVerdict::PENDING))) {
            return submission_id;
        }

        const RunnerCompileResult compile_result =
            runner->compile(execution_request);
        if (!compile_result.ok) {
            persist_result(
                result_store_, submission_id,
                make_result(submission_id, SubmissionStatus::FINISHED,
                            compile_result.verdict, compile_result.message));
            return submission_id;
        }

        if (!persist_result(result_store_, submission_id,
                            make_result(submission_id,
                                        SubmissionStatus::RUNNING,
                                        SubmissionVerdict::PENDING))) {
            return submission_id;
        }

        const std::vector<SubmissionCaseSpec> cases =
            load_cases_for_problem(request.pid);
        if (cases.empty()) {
            persist_result(result_store_, submission_id,
                           make_result(submission_id, SubmissionStatus::FAILED,
                                       SubmissionVerdict::SYSTEM_ERROR,
                                       "failed to load problem cases"));
            return submission_id;
        }

        SubmissionResult running_result =
            make_result(submission_id, SubmissionStatus::RUNNING,
                        SubmissionVerdict::PENDING);

        for (const SubmissionCaseSpec &case_spec : cases) {
            const RunnerCaseResult case_result =
                runner->runCase(execution_request, case_spec.input);
            running_result.case_results.push_back(case_result.result);
            running_result.message = case_result.message;
            if (!persist_result(result_store_, submission_id, running_result)) {
                return submission_id;
            }
        }

        SubmissionResult finished_result =
            make_result(submission_id, SubmissionStatus::FINISHED,
                        judge_core_.summarize(running_result.case_results),
                        running_result.message);
        finished_result.case_results = running_result.case_results;
        persist_result(result_store_, submission_id, finished_result);
    } catch (const std::exception &ex) {
        persist_result(result_store_, submission_id,
                       make_result(submission_id, SubmissionStatus::FAILED,
                                   SubmissionVerdict::SYSTEM_ERROR, ex.what()));
    }

    return submission_id;
}

bool SubmissionService::query(int submission_id,
                              SubmissionResult &result) const {
    return result_store_.getResult(submission_id, result);
}
