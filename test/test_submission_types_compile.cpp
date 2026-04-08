#include "common/SubmissionTypes.h"

static_assert(static_cast<int>(SubmissionLanguage::CPP) == 0);
static_assert(static_cast<int>(SubmissionLanguage::C) == 1);
static_assert(static_cast<int>(SubmissionLanguage::PYTHON) == 2);
static_assert(static_cast<int>(SubmissionVerdict::PENDING) == -1);
static_assert(static_cast<int>(SubmissionVerdict::AC) == 0);
static_assert(static_cast<int>(SubmissionVerdict::CE) == 7);
static_assert(static_cast<int>(SubmissionVerdict::UNKNOWN) == 8);

int main() {
    SubmissionRequest req{};
    req.uuid = 1;
    req.pid = "1000";
    req.language = SubmissionLanguage::CPP;
    req.code = "int main() { return 0; }";

    SubmissionResult result{};
    result.submission_id = 1;
    result.status = SubmissionStatus::QUEUED;
    result.verdict = SubmissionVerdict::PENDING;

    SubmissionCaseResult case_result{};
    case_result.seq_id = 1;
    case_result.verdict = SubmissionVerdict::AC;
    result.case_results.push_back(case_result);

    return result.case_results.empty();
}
