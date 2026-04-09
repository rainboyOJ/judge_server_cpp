#pragma once

#include <string>

#include "common/SubmissionTypes.h"

struct SubmissionTask {
  int submission_id{-1};
  SubmissionRequest request;
  std::string reply_channel_id;
};
