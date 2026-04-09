#pragma once

#include "common/SubmissionTypes.h"

class JudgeCore;
class ResultStore;
class RunnerFactory;

// SubmissionService 负责串联协议层、执行层、判题层和结果存储层。
class SubmissionService {
public:
  SubmissionService(ResultStore &result_store, RunnerFactory &runner_factory,
                    JudgeCore &judge_core);
  virtual ~SubmissionService() = default;

  int createSubmission(const SubmissionRequest &request);
  virtual void processSubmission(int submission_id,
                                 const SubmissionRequest &request);
  int submit(const SubmissionRequest &request);
  bool query(int submission_id, SubmissionResult &result) const;

private:
  ResultStore &result_store_;
  RunnerFactory &runner_factory_;
  JudgeCore &judge_core_;
};
