#include <cassert>
#include <vector>

#include "pipeline/JudgeCore.h"

namespace {

SubmissionCaseResult make_case_result(SubmissionVerdict verdict) {
    SubmissionCaseResult result{};
    result.verdict = verdict;
    return result;
}

void test_empty_input_returns_default_verdict() {
    JudgeCore judge_core;

    const SubmissionVerdict verdict =
        judge_core.summarize({}, SubmissionVerdict::UNKNOWN);

    assert(verdict == SubmissionVerdict::UNKNOWN);
}

void test_all_ac_returns_ac() {
    JudgeCore judge_core;
    const std::vector<SubmissionCaseResult> case_results = {
        make_case_result(SubmissionVerdict::AC),
        make_case_result(SubmissionVerdict::AC),
        make_case_result(SubmissionVerdict::AC),
    };

    const SubmissionVerdict verdict = judge_core.summarize(case_results);

    assert(verdict == SubmissionVerdict::AC);
}

void test_default_verdict_only_applies_to_empty_input() {
    JudgeCore judge_core;
    const std::vector<SubmissionCaseResult> case_results = {
        make_case_result(SubmissionVerdict::AC),
    };

    const SubmissionVerdict verdict =
        judge_core.summarize(case_results, SubmissionVerdict::CE);

    assert(verdict == SubmissionVerdict::AC);
}

void test_ce_dominates_all_other_verdicts() {
    JudgeCore judge_core;
    const std::vector<SubmissionCaseResult> case_results = {
        make_case_result(SubmissionVerdict::AC),
        make_case_result(SubmissionVerdict::WA),
        make_case_result(SubmissionVerdict::CE),
        make_case_result(SubmissionVerdict::TLE),
    };

    const SubmissionVerdict verdict = judge_core.summarize(case_results);

    assert(verdict == SubmissionVerdict::CE);
}

void test_tle_dominates_re_wa_and_ac() {
    JudgeCore judge_core;
    const std::vector<SubmissionCaseResult> case_results = {
        make_case_result(SubmissionVerdict::AC),
        make_case_result(SubmissionVerdict::WA),
        make_case_result(SubmissionVerdict::RE),
        make_case_result(SubmissionVerdict::TLE),
    };

    const SubmissionVerdict verdict = judge_core.summarize(case_results);

    assert(verdict == SubmissionVerdict::TLE);
}

void test_re_dominates_wa_and_ac_when_no_ce_or_tle() {
    JudgeCore judge_core;
    const std::vector<SubmissionCaseResult> case_results = {
        make_case_result(SubmissionVerdict::AC),
        make_case_result(SubmissionVerdict::WA),
        make_case_result(SubmissionVerdict::RE),
    };

    const SubmissionVerdict verdict = judge_core.summarize(case_results);

    assert(verdict == SubmissionVerdict::RE);
}

void test_wa_dominates_ac_when_no_more_severe_runtime_verdict_exists() {
    JudgeCore judge_core;
    const std::vector<SubmissionCaseResult> case_results = {
        make_case_result(SubmissionVerdict::AC),
        make_case_result(SubmissionVerdict::WA),
        make_case_result(SubmissionVerdict::AC),
    };

    const SubmissionVerdict verdict = judge_core.summarize(case_results);

    assert(verdict == SubmissionVerdict::WA);
}

void test_system_error_deterministically_dominates_unknown_and_ac() {
    JudgeCore judge_core;
    const std::vector<SubmissionCaseResult> case_results = {
        make_case_result(SubmissionVerdict::UNKNOWN),
        make_case_result(SubmissionVerdict::AC),
        make_case_result(SubmissionVerdict::SYSTEM_ERROR),
    };

    const SubmissionVerdict verdict = judge_core.summarize(case_results);

    assert(verdict == SubmissionVerdict::SYSTEM_ERROR);
}

void test_unknown_is_returned_when_it_is_the_only_non_ac_signal() {
    JudgeCore judge_core;
    const std::vector<SubmissionCaseResult> case_results = {
        make_case_result(SubmissionVerdict::UNKNOWN),
        make_case_result(SubmissionVerdict::AC),
    };

    const SubmissionVerdict verdict = judge_core.summarize(case_results);

    assert(verdict == SubmissionVerdict::UNKNOWN);
}

} // namespace

int main() {
    test_empty_input_returns_default_verdict();
    test_all_ac_returns_ac();
    test_default_verdict_only_applies_to_empty_input();
    test_ce_dominates_all_other_verdicts();
    test_tle_dominates_re_wa_and_ac();
    test_re_dominates_wa_and_ac_when_no_ce_or_tle();
    test_wa_dominates_ac_when_no_more_severe_runtime_verdict_exists();
    test_system_error_deterministically_dominates_unknown_and_ac();
    test_unknown_is_returned_when_it_is_the_only_non_ac_signal();
    return 0;
}
