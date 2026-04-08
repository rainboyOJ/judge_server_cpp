#include <atomic>
#include <cassert>
#include <thread>

#include "store/ResultStore.h"

namespace {

SubmissionResult make_result(int submission_id, SubmissionStatus status,
                             const char *message = "") {
    SubmissionResult result{};
    result.submission_id = submission_id;
    result.status = status;
    result.message = message;
    return result;
}

void test_status_transitions_follow_forward_only_order() {
    ResultStore store;
    SubmissionRequest request{};
    const int submission_id = store.createSubmission(request);

    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::PREPARING)));
    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::COMPILING)));
    assert(store.updateResult(
        submission_id, make_result(submission_id, SubmissionStatus::RUNNING)));
    assert(store.updateResult(
        submission_id, make_result(submission_id, SubmissionStatus::FINISHED)));

    SubmissionResult stored{};
    assert(store.getResult(submission_id, stored));
    assert(stored.status == SubmissionStatus::FINISHED);
}

void test_backward_status_transition_is_rejected() {
    ResultStore store;
    SubmissionRequest request{};
    const int submission_id = store.createSubmission(request);

    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::PREPARING)));
    assert(!store.updateResult(
        submission_id, make_result(submission_id, SubmissionStatus::QUEUED)));

    SubmissionResult stored{};
    assert(store.getResult(submission_id, stored));
    assert(stored.status == SubmissionStatus::PREPARING);
}

void test_compile_stage_can_finish_directly_for_compile_error() {
    ResultStore store;
    SubmissionRequest request{};
    const int submission_id = store.createSubmission(request);

    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::PREPARING)));
    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::COMPILING)));
    SubmissionResult compile_failed =
        make_result(submission_id, SubmissionStatus::FINISHED, "compile error");
    compile_failed.verdict = SubmissionVerdict::CE;
    assert(store.updateResult(submission_id, compile_failed));

    SubmissionResult stored{};
    assert(store.getResult(submission_id, stored));
    assert(stored.status == SubmissionStatus::FINISHED);
    assert(stored.verdict == SubmissionVerdict::CE);
}

void test_terminal_state_is_immutable() {
    ResultStore store;
    SubmissionRequest request{};
    const int submission_id = store.createSubmission(request);

    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::PREPARING)));
    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::COMPILING)));
    assert(store.updateResult(
        submission_id, make_result(submission_id, SubmissionStatus::RUNNING)));

    SubmissionResult finished =
        make_result(submission_id, SubmissionStatus::FINISHED, "done");
    finished.verdict = SubmissionVerdict::AC;
    assert(store.updateResult(submission_id, finished));

    SubmissionResult overwrite =
        make_result(submission_id, SubmissionStatus::FINISHED, "rewrite");
    overwrite.verdict = SubmissionVerdict::WA;
    assert(!store.updateResult(submission_id, overwrite));

    SubmissionResult stored{};
    assert(store.getResult(submission_id, stored));
    assert(stored.status == SubmissionStatus::FINISHED);
    assert(stored.verdict == SubmissionVerdict::AC);
    assert(stored.message == "done");
}

void test_missing_submission_lookup_returns_false() {
    ResultStore store;
    SubmissionResult stored{};
    assert(!store.getResult(9999, stored));
}

void test_concurrent_read_and_update_keeps_snapshots_valid() {
    ResultStore store;
    SubmissionRequest request{};
    const int submission_id = store.createSubmission(request);

    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::PREPARING)));
    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::COMPILING)));
    assert(store.updateResult(
        submission_id, make_result(submission_id, SubmissionStatus::RUNNING)));

    std::atomic<bool> start{false};
    std::atomic<bool> writer_done{false};

    std::thread writer([&]() {
        while (!start.load()) {
        }

        for (int i = 0; i < 128; ++i) {
            SubmissionResult update = make_result(
                submission_id, SubmissionStatus::RUNNING, "running");
            update.case_results.push_back(SubmissionCaseResult{
                1, SubmissionVerdict::AC, i, i, 64, 0, 0, 0});
            assert(store.updateResult(submission_id, update));
        }

        SubmissionResult finished =
            make_result(submission_id, SubmissionStatus::FINISHED, "done");
        finished.verdict = SubmissionVerdict::AC;
        finished.case_results.push_back(SubmissionCaseResult{
            1, SubmissionVerdict::AC, 128, 128, 64, 0, 0, 0});
        assert(store.updateResult(submission_id, finished));
        writer_done.store(true);
    });

    std::thread reader([&]() {
        start.store(true);

        while (!writer_done.load()) {
            SubmissionResult snapshot{};
            assert(store.getResult(submission_id, snapshot));
            assert(snapshot.submission_id == submission_id);
            assert(snapshot.status == SubmissionStatus::RUNNING ||
                   snapshot.status == SubmissionStatus::FINISHED);
            assert(snapshot.case_results.size() <= 1);
            if (!snapshot.case_results.empty()) {
                assert(snapshot.case_results.front().seq_id == 1);
            }
        }
    });

    writer.join();
    reader.join();

    SubmissionResult final_result{};
    assert(store.getResult(submission_id, final_result));
    assert(final_result.status == SubmissionStatus::FINISHED);
    assert(final_result.verdict == SubmissionVerdict::AC);
    assert(final_result.case_results.size() == 1);
    assert(final_result.case_results.front().cpu_time_ms == 128);
}

} // namespace

int main() {
    test_status_transitions_follow_forward_only_order();
    test_backward_status_transition_is_rejected();
    test_compile_stage_can_finish_directly_for_compile_error();
    test_terminal_state_is_immutable();
    test_missing_submission_lookup_returns_false();
    test_concurrent_read_and_update_keeps_snapshots_valid();
    return 0;
}
