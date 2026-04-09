#include "protocol/JudgeProtocol.h"

#include <exception>

#include "json.hpp"

using json = nlohmann::json;

namespace {

bool parseObjectPayload(const std::string &payload, json &parsed) {
  try {
    parsed = json::parse(payload);
  } catch (const std::exception &) {
    return false;
  }

  return parsed.is_object();
}

bool hasType(const json &parsed, const char *expected_type) {
  return parsed.contains("type") && parsed.at("type").is_string() &&
         parsed.at("type").get<std::string>() == expected_type;
}

SubmissionLanguage toSubmissionLanguage(int legacy_language, bool &ok) {
  ok = true;
  switch (legacy_language) {
  case 0:
    return SubmissionLanguage::CPP;
  case 1:
    return SubmissionLanguage::C;
  case 2:
    return SubmissionLanguage::PYTHON;
  }

  ok = false;
  return SubmissionLanguage::CPP;
}

const char *toStatusString(SubmissionStatus status) {
  switch (status) {
  case SubmissionStatus::QUEUED:
    return "QUEUED";
  case SubmissionStatus::PREPARING:
    return "PREPARING";
  case SubmissionStatus::COMPILING:
    return "COMPILING";
  case SubmissionStatus::RUNNING:
    return "RUNNING";
  case SubmissionStatus::FINISHED:
    return "FINISHED";
  case SubmissionStatus::FAILED:
    return "FAILED";
  }

  return "FAILED";
}

const char *toVerdictString(SubmissionVerdict verdict) {
  switch (verdict) {
  case SubmissionVerdict::PENDING:
    return "PENDING";
  case SubmissionVerdict::AC:
    return "AC";
  case SubmissionVerdict::WA:
    return "WA";
  case SubmissionVerdict::TLE:
    return "TLE";
  case SubmissionVerdict::MLE:
    return "MLE";
  case SubmissionVerdict::RE:
    return "RE";
  case SubmissionVerdict::OLE:
    return "OLE";
  case SubmissionVerdict::PE:
    return "PE";
  case SubmissionVerdict::CE:
    return "CE";
  case SubmissionVerdict::UNKNOWN:
    return "UNKNOWN";
  case SubmissionVerdict::SYSTEM_ERROR:
    return "SYSTEM_ERROR";
  }

  return "SYSTEM_ERROR";
}

json encodeCaseResult(const SubmissionCaseResult &case_result) {
  return json{{"seq_id", case_result.seq_id},
              {"verdict", toVerdictString(case_result.verdict)},
              {"cpu_time_ms", case_result.cpu_time_ms},
              {"real_time_ms", case_result.real_time_ms},
              {"memory_kb", case_result.memory_kb},
              {"signal", case_result.signal},
              {"exit_code", case_result.exit_code},
              {"error_code", case_result.error_code}};
}

json encodeAsyncEnvelope(const char *type, int submission_id,
                         SubmissionStatus status, SubmissionVerdict verdict,
                         const std::string &message) {
  return json{{"type", type},
              {"submission_id", submission_id},
              {"status", toStatusString(status)},
              {"verdict", toVerdictString(verdict)},
              {"message", message},
              {"case_results", json::array()}};
}

json encodeLegacyEnvelope(int submission_id, SubmissionStatus status,
                          SubmissionVerdict verdict, const std::string &message,
                          int code) {
  return json{{"submission_id", submission_id},
              {"status", toStatusString(status)},
              {"verdict", toVerdictString(verdict)},
              {"message", message},
              {"code", code},
              {"msg", message},
              {"case_results", json::array()}};
}

} // namespace

bool JudgeProtocol::decodeRequest(const std::string &payload,
                                  SubmissionRequest &request) const {
  json parsed;
  if (!parseObjectPayload(payload, parsed)) {
    return false;
  }

  if (!hasType(parsed, "submit") || !parsed.contains("uuid") ||
      !parsed.contains("pid") || !parsed.contains("lang") ||
      !parsed.contains("code")) {
    return false;
  }

  try {
    bool language_ok = false;
    const SubmissionLanguage language =
        toSubmissionLanguage(parsed.at("lang").get<int>(), language_ok);
    if (!language_ok) {
      return false;
    }

    request.uuid = parsed.at("uuid").get<int>();
    request.pid = parsed.at("pid").get<std::string>();
    request.language = language;
    request.code = parsed.at("code").get<std::string>();
  } catch (const std::exception &) {
    return false;
  }

  return true;
}

bool JudgeProtocol::decodeQueryRequest(const std::string &payload,
                                       QueryResultRequest &request) const {
  json parsed;
  if (!parseObjectPayload(payload, parsed)) {
    return false;
  }

  if (!hasType(parsed, "query_result") || !parsed.contains("submission_id")) {
    return false;
  }

  try {
    request.submission_id = parsed.at("submission_id").get<int>();
  } catch (const std::exception &) {
    return false;
  }

  return true;
}

std::string JudgeProtocol::encodeSubmissionAck(int submission_id) const {
  return encodeAsyncEnvelope("submission_ack", submission_id,
                             SubmissionStatus::QUEUED,
                             SubmissionVerdict::PENDING, "queued")
      .dump();
}

std::string
JudgeProtocol::encodeSubmissionUpdate(const SubmissionResult &result) const {
  json encoded =
      encodeAsyncEnvelope("submission_update", result.submission_id,
                          result.status, result.verdict, result.message);
  for (const auto &case_result : result.case_results) {
    encoded["case_results"].push_back(encodeCaseResult(case_result));
  }
  return encoded.dump();
}

std::string
JudgeProtocol::encodeSubmissionFinished(const SubmissionResult &result) const {
  json encoded =
      encodeAsyncEnvelope("submission_finished", result.submission_id,
                          result.status, result.verdict, result.message);
  for (const auto &case_result : result.case_results) {
    encoded["case_results"].push_back(encodeCaseResult(case_result));
  }
  return encoded.dump();
}

std::string JudgeProtocol::encodeResult(const SubmissionResult &result) const {
  if (result.status == SubmissionStatus::FINISHED ||
      result.status == SubmissionStatus::FAILED) {
    return encodeSubmissionFinished(result);
  }

  return encodeSubmissionUpdate(result);
}

std::string JudgeProtocol::encodeError(const std::string &message) const {
  json encoded =
      encodeLegacyEnvelope(-1, SubmissionStatus::FAILED,
                           SubmissionVerdict::SYSTEM_ERROR, message, -1);
  return encoded.dump();
}
