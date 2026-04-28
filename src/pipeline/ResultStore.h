#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "common/SubmissionTypes.h"

/**
 * @brief submission 最新结果快照的线程安全内存存储。
 *
 * 当前实现既负责：
 * - 生成 submission_id
 * - 保存最新 SubmissionResult
 * - 约束状态机只能向前推进
 * - 对终态结果做惰性清理
 */
class ResultStore {
public:
  /**
   * @brief 创建一条新的 submission 记录。
   * @return int 新生成的 submission_id。
   */
  int createSubmission(const SubmissionRequest &request);
  /**
   * @brief 更新某个 submission 的最新快照。
   * @return true 更新成功。
   * @return false submission 不存在或状态跳转非法。
   */
  bool updateResult(int submission_id, const SubmissionResult &result);
  /**
   * @brief 获取某个 submission 当前最新快照。
   */
  bool getResult(int submission_id, SubmissionResult &result) const;
  /** @brief 测试辅助：人为把记录“变老”。 */
  void debugForceAgingForTest(int submission_id, int age_seconds);
  /** @brief 测试辅助：覆盖清理参数。 */
  void debugConfigureCleanupForTest(int retention_seconds,
                                    int max_stored_results);

private:
  using Clock = std::chrono::steady_clock;

  /**
   * @brief ResultStore 内部保存的扩展记录。
   */
  struct StoredSubmission {
    SubmissionResult result;
    Clock::time_point last_access_at;
    std::optional<Clock::time_point> finished_at;
  };

  /** @brief 判断状态机跳转是否合法。 */
  static bool isValidTransition(SubmissionStatus current,
                                SubmissionStatus next);
  /** @brief 在持锁状态下执行惰性过期和容量清理。 */
  void cleanupExpiredResultsLocked(Clock::time_point now);
  /** @brief 读取当前生效的保留秒数配置。 */
  int resultRetentionSecondsLocked() const;
  /** @brief 读取当前生效的最大保存条数配置。 */
  int maxStoredResultsLocked() const;

  // 中文注释：状态机只允许按评测流程向前推进；终态写入后不再接受回退。
  mutable std::mutex mutex_;
  int next_submission_id_{1};
  std::unordered_map<int, StoredSubmission> results_;
  std::optional<int> override_retention_seconds_;
  std::optional<int> override_max_stored_results_;
};
