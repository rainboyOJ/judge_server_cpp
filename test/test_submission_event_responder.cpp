#include <cassert>
#include <optional>
#include <string>

#include "json.hpp"

#include "dispatch/SubmissionTask.h"
#include "network/SubmissionEventResponder.h"
#include "pipeline/JudgeCore.h"
#include "pipeline/ResultStore.h"
#include "pipeline/SubmissionService.h"
#include "protocol/JudgeProtocol.h"
#include "runner/RunnerFactory.h"

using json = nlohmann::json;

namespace {

SubmissionRequest make_submit_request() {
  SubmissionRequest request{};
  request.uuid = 95001;
  request.pid = "1000";
  request.language = SubmissionLanguage::PYTHON;
  request.code = "print(1)\n";
  return request;
}

SubmissionTask make_task(int submission_id) {
  SubmissionTask task{};
  task.submission_id = submission_id;
  task.request = make_submit_request();
  task.reply_channel_id = "slot:1:session:1";
  return task;
}

void require_status_transition(ResultStore &store, int submission_id,
                               SubmissionStatus status,
                               SubmissionVerdict verdict,
                               const std::string &message) {
  SubmissionResult current{};
  assert(store.getResult(submission_id, current));

  current.status = status;
  current.verdict = verdict;
  current.message = message;
  assert(store.updateResult(submission_id, current));
}

void test_started_event_for_queued_submission_returns_no_message() {
  ResultStore store;
  RunnerFactory factory;
  JudgeCore judge_core;
  SubmissionService service(store, factory, judge_core);
  JudgeProtocol protocol;
  SubmissionEventResponder responder(service, protocol);

  const int submission_id = service.createSubmission(make_submit_request());

  const std::optional<std::string> response =
      responder.onSubmissionStarted(make_task(submission_id));

  assert(!response.has_value());
}

void test_started_event_for_running_submission_returns_submission_update() {
  ResultStore store;
  RunnerFactory factory;
  JudgeCore judge_core;
  SubmissionService service(store, factory, judge_core);
  JudgeProtocol protocol;
  SubmissionEventResponder responder(service, protocol);

  const int submission_id = service.createSubmission(make_submit_request());
  require_status_transition(store, submission_id, SubmissionStatus::PREPARING,
                            SubmissionVerdict::PENDING, "preparing");
  require_status_transition(store, submission_id, SubmissionStatus::COMPILING,
                            SubmissionVerdict::PENDING, "compiling");
  require_status_transition(store, submission_id, SubmissionStatus::RUNNING,
                            SubmissionVerdict::PENDING, "running");

  const std::optional<std::string> response =
      responder.onSubmissionStarted(make_task(submission_id));

  assert(response.has_value());
  const json encoded = json::parse(*response);
  assert(encoded.at("type") == "submission_update");
  assert(encoded.at("submission_id") == submission_id);
  assert(encoded.at("status") == "RUNNING");
}

void test_finished_event_for_finished_submission_returns_submission_finished() {
  ResultStore store;
  RunnerFactory factory;
  JudgeCore judge_core;
  SubmissionService service(store, factory, judge_core);
  JudgeProtocol protocol;
  SubmissionEventResponder responder(service, protocol);

  const int submission_id = service.createSubmission(make_submit_request());
  require_status_transition(store, submission_id, SubmissionStatus::PREPARING,
                            SubmissionVerdict::PENDING, "preparing");
  require_status_transition(store, submission_id, SubmissionStatus::COMPILING,
                            SubmissionVerdict::PENDING, "compiling");
  require_status_transition(store, submission_id, SubmissionStatus::FINISHED,
                            SubmissionVerdict::AC, "accepted");

  const std::optional<std::string> response =
      responder.onSubmissionFinished(make_task(submission_id));

  assert(response.has_value());
  const json encoded = json::parse(*response);
  assert(encoded.at("type") == "submission_finished");
  assert(encoded.at("submission_id") == submission_id);
  assert(encoded.at("status") == "FINISHED");
  assert(encoded.at("verdict") == "AC");
}

void test_missing_submission_returns_no_message() {
  ResultStore store;
  RunnerFactory factory;
  JudgeCore judge_core;
  SubmissionService service(store, factory, judge_core);
  JudgeProtocol protocol;
  SubmissionEventResponder responder(service, protocol);

  const std::optional<std::string> response =
      responder.onSubmissionFinished(make_task(999999));

  assert(!response.has_value());
}

} // namespace

int main() {
  test_started_event_for_queued_submission_returns_no_message();
  test_started_event_for_running_submission_returns_submission_update();
  test_finished_event_for_finished_submission_returns_submission_finished();
  test_missing_submission_returns_no_message();
  return 0;
}
