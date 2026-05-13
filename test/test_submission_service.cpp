#include <cassert>
#include <string>

#include "pipeline/SubmissionVerdictReducer.h"
#include "pipeline/ResultStore.h"
#include "pipeline/SubmissionService.h"
#include "runner/RunnerFactory.h"

namespace {

SubmissionRequest make_python_request(int uuid, const std::string &code) {
  SubmissionRequest request{};
  request.uuid = uuid;
  request.pid = "1000";
  request.language = SubmissionLanguage::PYTHON;
  request.code = code;
  return request;
}

SubmissionRequest make_unsupported_request(int uuid) {
  SubmissionRequest request{};
  request.uuid = uuid;
  request.pid = "1000";
  request.language = SubmissionLanguage::C;
  request.code = "int main(){return 0;}\n";
  return request;
}

SubmissionService make_service(ResultStore &store, RunnerFactory &factory,
                               SubmissionVerdictReducer &judge_verdict_reducer) {
  return SubmissionService(store, factory, judge_verdict_reducer);
}

void test_create_submission_persists_queued_snapshot_before_processing() {
  ResultStore store;
  RunnerFactory factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService service = make_service(store, factory, judge_verdict_reducer);

  const int submission_id = service.createSubmission(make_python_request(
      93001, "a, b = map(int, input().split())\nprint(a + b)\n"));

  SubmissionResult stored{};
  assert(submission_id > 0);
  assert(service.query(submission_id, stored));
  assert(stored.submission_id == submission_id);
  assert(stored.status == SubmissionStatus::QUEUED);
  assert(stored.verdict == SubmissionVerdict::PENDING);
  assert(stored.case_results.empty());
}

void test_submit_async_creates_submission_and_hides_queue_details() {
  ResultStore store;
  RunnerFactory factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService service = make_service(store, factory, judge_verdict_reducer);

  const SubmissionRequest request = make_python_request(93005, "print(1)\n");
  const int submission_id = service.submitAsync(request, "reply-service");

  SubmissionTask task{};
  SubmissionResult stored{};
  assert(submission_id > 0);
  assert(service.queuedTaskCount() == 1);
  assert(service.query(submission_id, stored));
  assert(stored.status == SubmissionStatus::QUEUED);
  assert(service.waitTask(task));
  assert(task.submission_id == submission_id);
  assert(task.reply_channel_id == "reply-service");
  assert(task.request.uuid == 93005);
}

void test_process_submission_finishes_with_aggregated_ac_verdict() {
  ResultStore store;
  RunnerFactory factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService service = make_service(store, factory, judge_verdict_reducer);

  const SubmissionRequest request = make_python_request(
      93002, "a, b = map(int, input().split())\nprint(a + b)\n");
  const int submission_id = service.createSubmission(request);

  service.processSubmission(submission_id, request);

  SubmissionResult stored{};
  assert(service.query(submission_id, stored));
  assert(stored.status == SubmissionStatus::FINISHED);
  assert(stored.verdict == SubmissionVerdict::AC);
  assert(!stored.case_results.empty());
  assert(stored.case_results.front().verdict == SubmissionVerdict::AC);
}

void test_compile_failure_finishes_with_ce() {
  ResultStore store;
  RunnerFactory factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService service = make_service(store, factory, judge_verdict_reducer);

  const SubmissionRequest request =
      make_python_request(93003, "def broken(:\n    pass\n");
  const int submission_id = service.createSubmission(request);

  service.processSubmission(submission_id, request);

  SubmissionResult stored{};
  assert(service.query(submission_id, stored));
  assert(stored.status == SubmissionStatus::FINISHED);
  assert(stored.verdict == SubmissionVerdict::CE);
  assert(!stored.message.empty());
  assert(stored.case_results.empty());
}

void test_unsupported_language_finishes_as_deterministic_system_failure() {
  ResultStore store;
  RunnerFactory factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService service = make_service(store, factory, judge_verdict_reducer);

  const SubmissionRequest request = make_unsupported_request(93004);
  const int submission_id = service.createSubmission(request);

  service.processSubmission(submission_id, request);

  SubmissionResult stored{};
  assert(service.query(submission_id, stored));
  assert(stored.status == SubmissionStatus::FAILED);
  assert(stored.verdict == SubmissionVerdict::SYSTEM_ERROR);
  assert(!stored.message.empty());
}

void test_query_returns_false_for_missing_submission() {
  ResultStore store;
  RunnerFactory factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService service = make_service(store, factory, judge_verdict_reducer);

  SubmissionResult missing{};
  assert(!service.query(404404, missing));
}

} // namespace

int main() {
  test_create_submission_persists_queued_snapshot_before_processing();
  test_submit_async_creates_submission_and_hides_queue_details();
  test_process_submission_finishes_with_aggregated_ac_verdict();
  test_compile_failure_finishes_with_ce();
  test_unsupported_language_finishes_as_deterministic_system_failure();
  test_query_returns_false_for_missing_submission();
  return 0;
}
