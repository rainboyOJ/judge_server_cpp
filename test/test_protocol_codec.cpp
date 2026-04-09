#include <cassert>
#include <string>

#include "json.hpp"
#include "protocol/JudgeProtocol.h"

using json = nlohmann::json;

namespace {

void test_decode_request_uses_current_json_shape() {
  JudgeProtocol protocol;
  SubmissionRequest request{};

  const std::string payload =
      "{\"type\":\"submit\",\"uuid\":12345,\"pid\":\"P1001\","
      "\"lang\":2,\"code\":\"print('hello')\"}";

  const bool decoded = protocol.decodeRequest(payload, request);

  assert(decoded);
  assert(request.uuid == 12345);
  assert(request.pid == "P1001");
  assert(request.language == SubmissionLanguage::PYTHON);
  assert(request.code == "print('hello')");
}

void test_decode_request_preserves_long_pid_without_legacy_truncation() {
  JudgeProtocol protocol;
  SubmissionRequest request{};

  const std::string long_pid = "problem-id-that-is-definitely-longer-than-32";
  const std::string payload =
      std::string("{\"type\":\"submit\",\"uuid\":9,\"pid\":\"") + long_pid +
      "\",\"lang\":0,\"code\":\"int main(){}\"}";

  const bool decoded = protocol.decodeRequest(payload, request);

  assert(decoded);
  assert(request.pid == long_pid);
}

void test_decode_request_rejects_invalid_field_types_without_throwing() {
  JudgeProtocol protocol;
  SubmissionRequest request{};

  const std::string payload = "{\"type\":\"submit\",\"uuid\":1,\"pid\":123,"
                              "\"lang\":\"python\",\"code\":456}";

  const bool decoded = protocol.decodeRequest(payload, request);

  assert(!decoded);
}

void test_decode_query_request_accepts_submission_id() {
  JudgeProtocol protocol;
  QueryResultRequest request{};

  const std::string payload =
      "{\"type\":\"query_result\",\"submission_id\":77}";

  const bool decoded = protocol.decodeQueryRequest(payload, request);

  assert(decoded);
  assert(request.submission_id == 77);
}

void test_encode_submission_ack_emits_queued_message() {
  JudgeProtocol protocol;

  const json encoded = json::parse(protocol.encodeSubmissionAck(7));

  assert(encoded.at("type") == "submission_ack");
  assert(encoded.at("submission_id") == 7);
  assert(encoded.at("status") == "QUEUED");
}

void test_encode_submission_update_emits_stable_json_shape() {
  JudgeProtocol protocol;
  SubmissionResult result{};
  result.submission_id = 7;
  result.status = SubmissionStatus::RUNNING;
  result.verdict = SubmissionVerdict::PENDING;
  result.message = "running case 1";

  SubmissionCaseResult case_result{};
  case_result.seq_id = 1;
  case_result.verdict = SubmissionVerdict::AC;
  case_result.cpu_time_ms = 12;
  case_result.real_time_ms = 15;
  case_result.memory_kb = 256;
  case_result.signal = 0;
  case_result.exit_code = 0;
  case_result.error_code = 0;
  result.case_results.push_back(case_result);

  const json encoded = json::parse(protocol.encodeSubmissionUpdate(result));

  assert(encoded.at("type") == "submission_update");
  assert(encoded.at("submission_id") == 7);
  assert(encoded.at("status") == "RUNNING");
  assert(encoded.at("verdict") == "PENDING");
  assert(encoded.at("message") == "running case 1");
  assert(encoded.at("case_results").is_array());
  assert(encoded.at("case_results").size() == 1);
  assert(encoded.at("case_results")[0].at("seq_id") == 1);
  assert(encoded.at("case_results")[0].at("verdict") == "AC");
  assert(encoded.at("case_results")[0].at("cpu_time_ms") == 12);
}

void test_encode_submission_finished_emits_stable_json_shape() {
  JudgeProtocol protocol;
  SubmissionResult result{};
  result.submission_id = 7;
  result.status = SubmissionStatus::FINISHED;
  result.verdict = SubmissionVerdict::AC;
  result.message = "accepted";

  const json encoded = json::parse(protocol.encodeSubmissionFinished(result));

  assert(encoded.at("type") == "submission_finished");
  assert(encoded.at("submission_id") == 7);
  assert(encoded.at("status") == "FINISHED");
  assert(encoded.at("verdict") == "AC");
  assert(encoded.at("message") == "accepted");
  assert(encoded.at("case_results").is_array());
}

void test_encode_result_selects_update_for_non_terminal_status() {
  JudgeProtocol protocol;
  SubmissionResult result{};
  result.submission_id = 11;
  result.status = SubmissionStatus::RUNNING;
  result.verdict = SubmissionVerdict::PENDING;
  result.message = "still running";

  const json encoded = json::parse(protocol.encodeResult(result));

  assert(encoded.at("type") == "submission_update");
  assert(encoded.at("submission_id") == 11);
}

void test_encode_result_selects_finished_for_terminal_status() {
  JudgeProtocol protocol;
  SubmissionResult result{};
  result.submission_id = 12;
  result.status = SubmissionStatus::FINISHED;
  result.verdict = SubmissionVerdict::AC;
  result.message = "done";

  const json encoded = json::parse(protocol.encodeResult(result));

  assert(encoded.at("type") == "submission_finished");
  assert(encoded.at("submission_id") == 12);
}

void test_encode_query_result_returns_latest_in_progress_snapshot() {
  JudgeProtocol protocol;
  SubmissionResult result{};
  result.submission_id = 31;
  result.status = SubmissionStatus::RUNNING;
  result.verdict = SubmissionVerdict::PENDING;
  result.message = "running latest snapshot";

  const json encoded = json::parse(protocol.encodeResult(result));

  assert(encoded.at("type") == "submission_update");
  assert(encoded.at("submission_id") == 31);
  assert(encoded.at("status") == "RUNNING");
  assert(encoded.at("message") == "running latest snapshot");
}

void test_encode_query_result_returns_latest_terminal_snapshot() {
  JudgeProtocol protocol;
  SubmissionResult result{};
  result.submission_id = 32;
  result.status = SubmissionStatus::FINISHED;
  result.verdict = SubmissionVerdict::WA;
  result.message = "latest final snapshot";

  const json encoded = json::parse(protocol.encodeResult(result));

  assert(encoded.at("type") == "submission_finished");
  assert(encoded.at("submission_id") == 32);
  assert(encoded.at("status") == "FINISHED");
  assert(encoded.at("verdict") == "WA");
  assert(encoded.at("message") == "latest final snapshot");
}

void test_encode_error_emits_compatible_error_shape() {
  JudgeProtocol protocol;
  const json encoded = json::parse(protocol.encodeError("bad request"));

  assert(encoded.at("submission_id") == -1);
  assert(encoded.at("status") == "FAILED");
  assert(encoded.at("verdict") == "SYSTEM_ERROR");
  assert(encoded.at("message") == "bad request");
  assert(encoded.at("code") == -1);
  assert(encoded.at("msg") == "bad request");
  assert(encoded.at("case_results").is_array());
  assert(encoded.at("case_results").empty());
}

} // namespace

int main() {
  test_decode_request_uses_current_json_shape();
  test_decode_request_preserves_long_pid_without_legacy_truncation();
  test_decode_request_rejects_invalid_field_types_without_throwing();
  test_decode_query_request_accepts_submission_id();
  test_encode_submission_ack_emits_queued_message();
  test_encode_submission_update_emits_stable_json_shape();
  test_encode_submission_finished_emits_stable_json_shape();
  test_encode_result_selects_update_for_non_terminal_status();
  test_encode_result_selects_finished_for_terminal_status();
  test_encode_query_result_returns_latest_in_progress_snapshot();
  test_encode_query_result_returns_latest_terminal_snapshot();
  test_encode_error_emits_compatible_error_shape();
  return 0;
}
