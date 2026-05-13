#include <cassert>

#include "network/ClientSockets.h"
#include "pipeline/SubmissionVerdictReducer.h"
#include "pipeline/ResultStore.h"
#include "pipeline/SubmissionService.h"
#include "runner/RunnerFactory.h"

namespace {

void test_client_sockets_accepts_injected_submission_service() {
  ResultStore result_store;
  RunnerFactory runner_factory;
  SubmissionVerdictReducer judge_verdict_reducer;
  SubmissionService submission_service(result_store, runner_factory,
                                       judge_verdict_reducer);

  ClientSockets client_sockets(16, submission_service);

  assert(&client_sockets.submission_service() == &submission_service);
}

} // namespace

int main() {
  test_client_sockets_accepts_injected_submission_service();
  return 0;
}
