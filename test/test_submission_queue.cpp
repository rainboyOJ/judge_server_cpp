#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "dispatch/SubmissionQueue.h"

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

void test_queue_preserves_fifo_order() {
  SubmissionQueue queue;
  queue.push(make_task(101, 201, "reply-a"));
  queue.push(make_task(102, 202, "reply-b"));

  SubmissionTask first{};
  SubmissionTask second{};
  assert(queue.pop(first));
  assert(queue.pop(second));

  assert(first.submission_id == 101);
  assert(first.request.uuid == 201);
  assert(first.reply_channel_id == "reply-a");
  assert(second.submission_id == 102);
  assert(second.request.uuid == 202);
  assert(second.reply_channel_id == "reply-b");
}

void test_pop_unblocks_and_returns_false_after_shutdown() {
  SubmissionQueue queue;
  SubmissionTask popped{};
  std::atomic<bool> pop_returned{false};
  std::atomic<bool> pop_result{true};
  std::mutex started_mutex;
  std::condition_variable started_cv;
  bool worker_started = false;

  std::thread worker([&]() {
    {
      std::lock_guard<std::mutex> lock(started_mutex);
      worker_started = true;
    }
    started_cv.notify_one();
    pop_result = queue.pop(popped);
    pop_returned = true;
  });

  {
    std::unique_lock<std::mutex> lock(started_mutex);
    started_cv.wait(lock, [&]() { return worker_started; });
  }
  assert(!pop_returned.load());

  queue.shutdown();
  worker.join();

  assert(pop_returned.load());
  assert(!pop_result.load());
}

void test_pop_returns_same_submission_id_that_was_enqueued() {
  SubmissionQueue queue;
  const int submission_id = 987654;

  queue.push(make_task(submission_id, 301, "reply-same-id"));

  SubmissionTask popped{};
  assert(queue.pop(popped));
  assert(popped.submission_id == submission_id);
}

} // namespace

int main() {
  test_queue_preserves_fifo_order();
  test_pop_unblocks_and_returns_false_after_shutdown();
  test_pop_returns_same_submission_id_that_was_enqueued();
  return 0;
}
