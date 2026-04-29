#include <cassert>
#include <string>

#include "json.hpp"

#include "network/QueryRequestHandler.h"
#include "pipeline/JudgeCore.h"
#include "pipeline/ResultStore.h"
#include "pipeline/SubmissionService.h"
#include "protocol/JudgeProtocol.h"
#include "runner/RunnerFactory.h"

using json = nlohmann::json;

namespace {

SubmissionRequest make_submit_request() {
  SubmissionRequest request{};
  request.uuid = 94002;
  request.pid = "1000";
  request.language = SubmissionLanguage::PYTHON;
  request.code = "print(1)\n";
  return request;
}

void test_handle_query_returns_encoded_snapshot_for_existing_submission() {
  ResultStore store;
  RunnerFactory factory;
  JudgeCore judge_core;
  SubmissionService service(store, factory, judge_core);
  JudgeProtocol protocol;
  QueryRequestHandler handler(service, protocol);

  const int submission_id = service.createSubmission(make_submit_request());
  QueryResultRequest request{};
  request.submission_id = submission_id;

  const json encoded = json::parse(handler.handleQuery(request));

  assert(encoded.at("type").is_string());
  const std::string response_type = encoded.at("type").get<std::string>();
  assert(response_type == "submission_update" ||
         response_type == "submission_finished");
  assert(encoded.at("submission_id") == submission_id);
}

void test_handle_query_returns_submission_not_found_error_for_missing_submission() {
  ResultStore store;
  RunnerFactory factory;
  JudgeCore judge_core;
  SubmissionService service(store, factory, judge_core);
  JudgeProtocol protocol;
  QueryRequestHandler handler(service, protocol);

  QueryResultRequest request{};
  request.submission_id = 999999;

  const json encoded = json::parse(handler.handleQuery(request));

  assert(encoded.at("submission_id") == -1);
  assert(encoded.at("status") == "FAILED");
  assert(encoded.at("verdict") == "SYSTEM_ERROR");
  assert(encoded.at("message") == "submission not found");
  assert(encoded.at("msg") == "submission not found");
}

} // namespace

int main() {
  test_handle_query_returns_encoded_snapshot_for_existing_submission();
  test_handle_query_returns_submission_not_found_error_for_missing_submission();
  return 0;
}
