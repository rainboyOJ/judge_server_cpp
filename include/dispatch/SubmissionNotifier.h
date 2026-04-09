#pragma once

#include "dispatch/SubmissionTask.h"

class SubmissionNotifier {
public:
  virtual ~SubmissionNotifier() = default;

  virtual void onSubmissionStarted(const SubmissionTask &task) { (void)task; }

  virtual void onSubmissionFinished(const SubmissionTask &task) { (void)task; }
};
