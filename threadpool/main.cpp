#include <iostream>
#include <vector>

#include "threadpool.h"

static std::recursive_mutex log_mutex;

void log()
{
  std::lock_guard<std::recursive_mutex> lock { log_mutex };
  std::cout << std::endl;
}

template <typename Left, typename... Right>
void log(Left&& left, Right&&... right)
{
  std::lock_guard<std::recursive_mutex> lock { log_mutex };
  std::cout << std::forward<Left>(left);
  log(std::forward<Right>(right)...);
}


int main(int argc, const char * argv[]) {
  ThreadPool tp;
  auto future1 = tp.queue_task([]() -> int {
    log("Queued 1");
    return 1;
  });
  auto future2 = tp.queue_task([] {
    log("Queued 2");
    return std::shared_ptr<std::string>(new std::string("2"));
  });
  tp.queue_task([] {
    log("Queued 3");
//    throw 1;
  });

  log("Future 1: ", future1->get_result());
  log("Future 2: ", *future2->get_result());
  log("Future 2: ", *future2->get_result());

  tp.queue_task([] {
    log("Queued 4");
//    return std::string { "Hello World" };
  });

  log("queueing a bunch of jobs");
  std::vector<std::shared_ptr<IFutureResult<int>>> future_results;
  for (int i = 0; i < 10000; i++)
  {
    std::string msg { "queued " };
    msg += std::to_string(i);
    future_results.emplace_back(tp.queue_task([msg]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      return 1;
    }));
  }

  std::vector<int> results;
  for (const auto& future_result: future_results)
  {
    results.push_back(future_result->get_result());
  }
  // Not a great idea, just do it in the main thread
  auto sum_future = tp.queue_task([&](){
    auto sum = 0;
    for (auto i: results)
      sum += i;
    return sum;
  });
  log("Sum is ", sum_future->get_result());

  tp.queue_task([] {
    log("Queued 5");
  });
  tp.queue_task([] {
    log("Queued 6");
  });
  tp.queue_task([] {
    log("Queued 7");
  });
  tp.queue_task([] {
    log("Queued 8");
  });
  tp.queue_task([] {
    log("Queued 9");
  });
  tp.queue_task([] {
    log("Queued 10");
  });
  tp.queue_task([] {
    log("Queued 11");
  });

  tp.join();
  return 0;
}
