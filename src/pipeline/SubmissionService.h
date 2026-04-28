#pragma once

#include "common/SubmissionTypes.h"

class JudgeCore;
class ResultStore;
class RunnerFactory;

/**
 * @brief 异步评测后端的核心编排服务。
 *
 * 它负责：
 * - 创建 submission 记录
 * - 推进状态机
 * - 选择 runner 执行代码
 * - 汇总 verdict
 * - 持久化到 ResultStore
 */
class SubmissionService {
public:
  SubmissionService(ResultStore &result_store, RunnerFactory &runner_factory,
                    JudgeCore &judge_core);
  virtual ~SubmissionService() = default;

  /**
   * @brief 创建一条新的 submission 记录，并返回服务端生成的 submission_id。
   */
  int createSubmission(const SubmissionRequest &request);
  /**
   * @brief 执行一份 submission 的完整评测流程。
   * @param submission_id 由服务端生成的唯一 submission 编号。
   * @param request 原始提交请求。
   */
  virtual void processSubmission(int submission_id,
                                 const SubmissionRequest &request);
  /**
   * @brief 同步兼容入口：创建后立刻执行。
   *
   * @note 当前异步主链路不再依赖它，但保留它可兼容旧测试或同步调用场景。
   */
  int submit(const SubmissionRequest &request);
  /**
   * @brief 查询某个 submission 当前最新结果快照。
   */
  bool query(int submission_id, SubmissionResult &result) const;

private:
  ResultStore &result_store_;
  RunnerFactory &runner_factory_;
  JudgeCore &judge_core_;
};
