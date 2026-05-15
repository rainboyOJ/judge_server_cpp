#pragma once

#include "common/SubmissionTypes.h"
#include "dispatch/SubmissionQueue.h"
#include "dispatch/SubmissionTask.h"

class SubmissionVerdictReducer;
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
                    SubmissionVerdictReducer &judge_verdict_reducer);
  virtual ~SubmissionService() = default;

  /**
   * @brief 创建一条新的 submission 记录，并返回服务端生成的 submission_id。
   */
  int createSubmission(const SubmissionRequest &request);

  /**
   * @brief 创建 submission 并投递到内部异步队列。
   *
   * 网络层只需要表达“提交这份代码，并把后续消息回推到哪个 reply channel”，
   * 不需要知道后台队列的存在。
   *
   * 这里是“网络线程把评测任务交给后台 worker”的真正边界：
   * submitAsync() 会创建 SubmissionTask 并压入 SubmissionQueue，
   * 后续由 JudgeWorkerPool 的 worker 线程通过 waitTask()/pop() 取走执行。
   */
  int submitAsync(const SubmissionRequest &request,
                  const std::string &reply_channel_id);

  /** @brief worker 从内部队列阻塞获取任务。 */
  bool waitTask(SubmissionTask &task);

  /** @brief 关闭内部队列并唤醒 worker。 */
  void shutdownQueue();

  /** @brief 暴露队列长度，主要用于测试和运行时观测。 */
  std::size_t queuedTaskCount() const;

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
  SubmissionVerdictReducer &judge_verdict_reducer_;
  SubmissionQueue queue_;
};
