#include <cassert>
#include <string>

#include "json.hpp"

#include "dispatch/SubmissionTask.h"
#include "network/AckBarrier.h"
#include "network/SubmissionRequestHandler.h"
#include "pipeline/SubmissionVerdictReducer.h"
#include "pipeline/ResultStore.h"
#include "pipeline/SubmissionService.h"
#include "protocol/JudgeProtocol.h"
#include "runner/RunnerFactory.h"

using json = nlohmann::json;

namespace {

SubmissionRequest make_submit_request() {
  SubmissionRequest request{};
  request.uuid = 94001;
  request.pid = "1000";
  request.language = SubmissionLanguage::PYTHON;
  request.code = "print(1)\n";
  return request;
}

void test_handle_submit_returns_submission_ack_and_queues_task_for_same_reply_channel() {
  ResultStore store;
  RunnerFactory factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService service(store, factory, judge_verdict_reducer);
  JudgeProtocol protocol;
  AckBarrier ack_barrier;
  SubmissionRequestHandler handler(service, protocol, ack_barrier);

  const std::string reply_channel_id = "slot-7:session-11";
  const SubmissionRequestHandler::HandleSubmitResult result =
      handler.handleSubmit(make_submit_request(), reply_channel_id);

  const json encoded = json::parse(result.response);
  assert(encoded.at("type") == "submission_ack");
  assert(result.deferred_messages.empty());

  const int submission_id = encoded.at("submission_id").get<int>();
  assert(submission_id > 0);

  SubmissionTask task{};
  assert(service.waitTask(task));
  assert(task.reply_channel_id == reply_channel_id);
  assert(task.submission_id == submission_id);
}

void test_handle_submit_releases_ack_barrier_and_returns_deferred_messages() {
  ResultStore store;
  RunnerFactory factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService service(store, factory, judge_verdict_reducer);
  JudgeProtocol protocol;
  AckBarrier ack_barrier;
  SubmissionRequestHandler handler(service, protocol, ack_barrier);

  const std::string reply_channel_id = "slot-8:session-21";
  ack_barrier.mark_waiting(reply_channel_id);
  assert(ack_barrier.try_defer(reply_channel_id, "deferred-update"));

  const SubmissionRequestHandler::HandleSubmitResult result =
      handler.handleSubmit(make_submit_request(), reply_channel_id);

  const json encoded = json::parse(result.response);
  assert(encoded.at("type") == "submission_ack");
  assert(result.deferred_messages.size() == 1);
  assert(result.deferred_messages.front() == "deferred-update");
  assert(!ack_barrier.try_defer(reply_channel_id, "late-update"));
}

void test_handle_submit_returns_queue_unavailable_error_when_service_queue_is_shutdown() {
  ResultStore store;
  RunnerFactory factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService service(store, factory, judge_verdict_reducer);
  JudgeProtocol protocol;
  AckBarrier ack_barrier;
  SubmissionRequestHandler handler(service, protocol, ack_barrier);

  service.shutdownQueue();

  const SubmissionRequestHandler::HandleSubmitResult result =
      handler.handleSubmit(make_submit_request(), "slot-9:session-2");

  const json encoded = json::parse(result.response);
  assert(encoded.at("submission_id") == -1);
  assert(encoded.at("status") == "FAILED");
  assert(encoded.at("message") == "submission queue unavailable");
  assert(result.deferred_messages.empty());
}

} // namespace

int main() {
  test_handle_submit_returns_submission_ack_and_queues_task_for_same_reply_channel();
  test_handle_submit_releases_ack_barrier_and_returns_deferred_messages();
  test_handle_submit_returns_queue_unavailable_error_when_service_queue_is_shutdown();
  return 0;
}
