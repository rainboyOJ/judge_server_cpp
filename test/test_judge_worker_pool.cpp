#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>

#include "dispatch/JudgeWorkerPool.h"
#include "dispatch/SubmissionNotifier.h"
#include "pipeline/JudgeCore.h"
#include "pipeline/ResultStore.h"
#include "pipeline/SubmissionService.h"
#include "runner/RunnerFactory.h"

namespace {

SubmissionTask make_task(int submission_id, int request_uuid,
                         const std::string &reply_channel_id) {
  SubmissionTask task{};
  task.submission_id = submission_id;
  task.request.uuid = request_uuid;
  task.request.pid = "1000";
  task.request.language = SubmissionLanguage::PYTHON;
  task.request.code = "print(1)\n";
  task.reply_channel_id = reply_channel_id;
  return task;
}

class RecordingNotifier : public SubmissionNotifier {
public:
  void onSubmissionStarted(const SubmissionTask &task) override {
    std::lock_guard<std::mutex> lock(mutex_);
    started_count_++;
    last_started_submission_id_ = task.submission_id;
  }

  void onSubmissionFinished(const SubmissionTask &task) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      finished_count_++;
      last_finished_submission_id_ = task.submission_id;
    }
    condition_.notify_all();
  }

  bool waitForFinishedCount(int expected_count,
                            std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return condition_.wait_for(
        lock, timeout, [&]() { return finished_count_ >= expected_count; });
  }

  int startedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return started_count_;
  }

  int finishedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return finished_count_;
  }

  int lastStartedSubmissionId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_started_submission_id_;
  }

  int lastFinishedSubmissionId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_finished_submission_id_;
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  int started_count_{0};
  int finished_count_{0};
  int last_started_submission_id_{-1};
  int last_finished_submission_id_{-1};
};

class FakeSubmissionService : public SubmissionService {
public:
  FakeSubmissionService();

  void processSubmission(int submission_id,
                         const SubmissionRequest &request) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      call_count_++;
      last_submission_id_ = submission_id;
      last_request_uuid_ = request.uuid;
    }
    condition_.notify_all();
  }

  bool waitForCallCount(int expected_count, std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex_);
    return condition_.wait_for(lock, timeout,
                               [&]() { return call_count_ >= expected_count; });
  }

  int callCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return call_count_;
  }

  int lastSubmissionId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_submission_id_;
  }

  int lastRequestUuid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_request_uuid_;
  }

private:
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  int call_count_{0};
  int last_submission_id_{-1};
  int last_request_uuid_{-1};
};

ResultStore &fake_result_store() {
  static ResultStore store;
  return store;
}

RunnerFactory &fake_runner_factory() {
  static RunnerFactory factory;
  return factory;
}

JudgeCore &fake_judge_core() {
  static JudgeCore judge_core;
  return judge_core;
}

FakeSubmissionService::FakeSubmissionService()
    : SubmissionService(fake_result_store(), fake_runner_factory(),
                        fake_judge_core()) {}

void test_worker_pool_consumes_queued_task() {
  FakeSubmissionService service;
  RecordingNotifier notifier;

  {
    JudgeWorkerPool pool(1, service, &notifier);
    service.submitAsync(make_task(321, 654, "reply-consume").request,
                        "reply-consume");

    assert(service.waitForCallCount(1, std::chrono::milliseconds(1000)));
    assert(service.queuedTaskCount() == 0);
  }
}

void test_worker_pool_invokes_processing_and_notifier() {
  FakeSubmissionService service;
  RecordingNotifier notifier;

  {
    JudgeWorkerPool pool(1, service, &notifier);
    const int submission_id = service.submitAsync(
        make_task(777, 888, "reply-notify").request, "reply-notify");

    assert(service.waitForCallCount(1, std::chrono::milliseconds(1000)));
    assert(notifier.waitForFinishedCount(1, std::chrono::milliseconds(1000)));
    assert(service.callCount() >= 1);
    assert(service.lastSubmissionId() == submission_id);
    assert(service.lastRequestUuid() == 888);
    assert(notifier.startedCount() == 1);
    assert(notifier.finishedCount() == 1);
    assert(notifier.lastStartedSubmissionId() == submission_id);
    assert(notifier.lastFinishedSubmissionId() == submission_id);
  }
}

void test_is_stopping_after_stop() {
  FakeSubmissionService service;
  RecordingNotifier notifier;

  JudgeWorkerPool pool(1, service, &notifier);
  assert(!pool.is_stopping());

  pool.stop();
  assert(pool.is_stopping());
}

} // namespace

int main() {
  test_worker_pool_consumes_queued_task();
  test_worker_pool_invokes_processing_and_notifier();
  test_is_stopping_after_stop();
  return 0;
}
