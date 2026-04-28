#include <atomic>
#include <cassert>
#include <thread>

#include "common/Config.h"

#include "pipeline/ResultStore.h"

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
      submission_id, make_result(submission_id, SubmissionStatus::PREPARING)));
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::COMPILING)));
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
      submission_id, make_result(submission_id, SubmissionStatus::PREPARING)));
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
      submission_id, make_result(submission_id, SubmissionStatus::PREPARING)));
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::COMPILING)));
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
      submission_id, make_result(submission_id, SubmissionStatus::PREPARING)));
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::COMPILING)));
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

void test_get_result_returns_newest_running_snapshot() {
  ResultStore store;
  SubmissionRequest request{};
  const int submission_id = store.createSubmission(request);

  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::PREPARING)));
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::COMPILING)));

  SubmissionResult first_running =
      make_result(submission_id, SubmissionStatus::RUNNING, "running 1");
  first_running.case_results.push_back(
      SubmissionCaseResult{1, SubmissionVerdict::AC, 10, 10, 64, 0, 0, 0});
  assert(store.updateResult(submission_id, first_running));

  SubmissionResult second_running =
      make_result(submission_id, SubmissionStatus::RUNNING, "running 2");
  second_running.case_results.push_back(
      SubmissionCaseResult{1, SubmissionVerdict::AC, 10, 10, 64, 0, 0, 0});
  second_running.case_results.push_back(
      SubmissionCaseResult{2, SubmissionVerdict::WA, 20, 21, 96, 0, 1, 0});
  assert(store.updateResult(submission_id, second_running));

  SubmissionResult queried{};
  assert(store.getResult(submission_id, queried));
  assert(queried.status == SubmissionStatus::RUNNING);
  assert(queried.message == "running 2");
  assert(queried.case_results.size() == 2);
  assert(queried.case_results.back().seq_id == 2);
  assert(queried.case_results.back().verdict == SubmissionVerdict::WA);
}

void test_get_result_keeps_final_snapshot_after_failed_delivery() {
  ResultStore store;
  SubmissionRequest request{};
  const int submission_id = store.createSubmission(request);

  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::PREPARING)));
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::COMPILING)));
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::RUNNING)));

  SubmissionResult finished =
      make_result(submission_id, SubmissionStatus::FINISHED, "accepted");
  finished.verdict = SubmissionVerdict::AC;
  finished.case_results.push_back(
      SubmissionCaseResult{1, SubmissionVerdict::AC, 12, 12, 64, 0, 0, 0});
  assert(store.updateResult(submission_id, finished));

  SubmissionResult disconnected_push =
      make_result(submission_id, SubmissionStatus::FAILED, "socket closed");
  disconnected_push.verdict = SubmissionVerdict::SYSTEM_ERROR;
  assert(!store.updateResult(submission_id, disconnected_push));

  SubmissionResult queried{};
  assert(store.getResult(submission_id, queried));
  assert(queried.status == SubmissionStatus::FINISHED);
  assert(queried.verdict == SubmissionVerdict::AC);
  assert(queried.message == "accepted");
  assert(queried.case_results.size() == 1);
  assert(queried.case_results.front().seq_id == 1);
}

void test_concurrent_read_and_update_keeps_snapshots_valid() {
  ResultStore store;
  SubmissionRequest request{};
  const int submission_id = store.createSubmission(request);

  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::PREPARING)));
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::COMPILING)));
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::RUNNING)));

  std::atomic<bool> start{false};
  std::atomic<bool> writer_done{false};

  std::thread writer([&]() {
    while (!start.load()) {
    }

    for (int i = 0; i < 128; ++i) {
      SubmissionResult update =
          make_result(submission_id, SubmissionStatus::RUNNING, "running");
      update.case_results.push_back(
          SubmissionCaseResult{1, SubmissionVerdict::AC, i, i, 64, 0, 0, 0});
      assert(store.updateResult(submission_id, update));
    }

    SubmissionResult finished =
        make_result(submission_id, SubmissionStatus::FINISHED, "done");
    finished.verdict = SubmissionVerdict::AC;
    finished.case_results.push_back(
        SubmissionCaseResult{1, SubmissionVerdict::AC, 128, 128, 64, 0, 0, 0});
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

void test_expired_finished_results_are_removed_when_creating_new_submission() {
  ResultStore store;
  store.debugConfigureCleanupForTest(1, 1000);

  SubmissionRequest request{};
  const int first_submission_id = store.createSubmission(request);
  assert(store.updateResult(
      first_submission_id,
      make_result(first_submission_id, SubmissionStatus::PREPARING)));
  assert(store.updateResult(
      first_submission_id,
      make_result(first_submission_id, SubmissionStatus::COMPILING)));
  assert(store.updateResult(
      first_submission_id,
      make_result(first_submission_id, SubmissionStatus::RUNNING)));

  SubmissionResult finished =
      make_result(first_submission_id, SubmissionStatus::FINISHED, "done");
  finished.verdict = SubmissionVerdict::AC;
  assert(store.updateResult(first_submission_id, finished));

  store.debugForceAgingForTest(first_submission_id, 1000);

  const int second_submission_id = store.createSubmission(request);
  assert(second_submission_id > first_submission_id);

  SubmissionResult stale{};
  assert(!store.getResult(first_submission_id, stale));

  SubmissionResult live{};
  assert(store.getResult(second_submission_id, live));
  assert(live.status == SubmissionStatus::QUEUED);
}

void test_running_results_are_not_removed_even_if_forced_old() {
  ResultStore store;
  store.debugConfigureCleanupForTest(1, 1000);

  SubmissionRequest request{};
  const int submission_id = store.createSubmission(request);
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::PREPARING)));
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::COMPILING)));
  assert(store.updateResult(
      submission_id, make_result(submission_id, SubmissionStatus::RUNNING)));

  store.debugForceAgingForTest(submission_id, 1000);

  const int second_submission_id = store.createSubmission(request);
  assert(second_submission_id > submission_id);

  SubmissionResult running{};
  assert(store.getResult(submission_id, running));
  assert(running.status == SubmissionStatus::RUNNING);
}

void test_capacity_cleanup_keeps_newest_finished_results() {
  ResultStore store;
  store.debugConfigureCleanupForTest(3600, 3);

  SubmissionRequest request{};
  const int first = store.createSubmission(request);
  const int second = store.createSubmission(request);
  const int third = store.createSubmission(request);

  const auto finish_submission = [&](int submission_id, const char *message) {
    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::PREPARING, message)));
    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::COMPILING, message)));
    assert(store.updateResult(
        submission_id,
        make_result(submission_id, SubmissionStatus::RUNNING, message)));
    SubmissionResult finished =
        make_result(submission_id, SubmissionStatus::FINISHED, message);
    finished.verdict = SubmissionVerdict::AC;
    assert(store.updateResult(submission_id, finished));
  };

  finish_submission(first, "first");
  finish_submission(second, "second");
  finish_submission(third, "third");

  store.debugForceAgingForTest(first, 3);
  store.debugForceAgingForTest(second, 2);
  store.debugForceAgingForTest(third, 1);

  const int fourth = store.createSubmission(request);
  assert(fourth > third);

  SubmissionResult result{};
  assert(!store.getResult(first, result));
  assert(store.getResult(second, result));
  assert(store.getResult(third, result));
}

} // namespace

int main() {
  test_status_transitions_follow_forward_only_order();
  test_backward_status_transition_is_rejected();
  test_compile_stage_can_finish_directly_for_compile_error();
  test_terminal_state_is_immutable();
  test_missing_submission_lookup_returns_false();
  test_get_result_returns_newest_running_snapshot();
  test_get_result_keeps_final_snapshot_after_failed_delivery();
  test_concurrent_read_and_update_keeps_snapshots_valid();
  test_expired_finished_results_are_removed_when_creating_new_submission();
  test_running_results_are_not_removed_even_if_forced_old();
  test_capacity_cleanup_keeps_newest_finished_results();
  return 0;
}
