#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <thread>

class IFuture
{
public:
  virtual void run() noexcept = 0;
  virtual void wait() noexcept = 0;
};

template <typename Functor>
class FutureBase
: public IFuture
{
public:
  FutureBase(Functor&& functor) noexcept
  : m_functor { std::forward<Functor>(functor) }
  {
  }

  virtual void run() noexcept override
  {
    std::lock_guard<std::mutex> lock { m_mutex };
    run_internal();
    m_done = true;
    m_condition.notify_all();
  }

  virtual void wait() noexcept override
  {
    std::unique_lock<std::mutex> lock { m_mutex };
    m_condition.wait(lock, [this]() -> bool { return m_done; });
  }

protected:
  virtual void run_internal() noexcept = 0;

  Functor m_functor;
  bool m_done { false };
  std::condition_variable m_condition;
  std::mutex m_mutex;
};

template <typename TResult>
class IFutureResult
{
public:
  virtual const TResult& get_result() noexcept = 0;
};

template <typename Functor, typename TResult>
class Future
: public FutureBase<Functor>, public IFutureResult<TResult>
{
public:
  using FutureBase<Functor>::FutureBase;
  using FutureBase<Functor>::wait;

  virtual const TResult& get_result() noexcept override
  {
    FutureBase<Functor>::wait();
    return m_result;
  }


protected:
  virtual void run_internal() noexcept override
  {
    m_result = std::move(FutureBase<Functor>::m_functor());
  }

  TResult m_result;
};

template <typename Functor>
class Future<Functor, void>
: public FutureBase<Functor>
{
public:
  using FutureBase<Functor>::FutureBase;
protected:
  virtual void run_internal() noexcept override
  {
    FutureBase<Functor>::m_functor();
  }
};

class ThreadPool
{
public:
  ThreadPool(std::size_t n_workers = std::thread::hardware_concurrency()) noexcept
    : m_max_worker_count { n_workers }
  {
  }

  template <typename Functor>
  std::shared_ptr<Future<Functor, decltype(std::declval<Functor>()())>> queue_task(Functor&& task) noexcept
  {
    std::lock_guard<std::recursive_mutex> lock { m_queue_mutex };
    std::shared_ptr<Future<Functor, decltype(std::declval<Functor>()())>> future { new Future<Functor, decltype(std::declval<Functor>()())> { std::move(task) } };
    m_queue.emplace_back(future);
    m_queue_condition.notify_one();
    start_workers();
    return future;
  }


  std::shared_ptr<IFuture> pop_task() noexcept
  {
    std::unique_lock<std::recursive_mutex> lock { m_queue_mutex };
    if (m_queue.empty())
      return nullptr;

    auto task = std::move(m_queue.front());
    m_queue.pop_front();
    return task;
  }

  void worker() noexcept
  {
    while (true)
    {
      const auto task = pop_task();
      if (!task)
      {
        std::lock_guard<std::mutex> lock { m_worker_count_mutex };
        m_worker_count--;
        m_worker_count_condition.notify_all();
        return;
      }
      task->run();
    }
  }

  // will never return unless all threads crash
  void join() noexcept
  {
    std::unique_lock<std::mutex> lock { m_worker_count_mutex };
    m_worker_count_condition.wait(lock, [this]() { return m_worker_count == 0; });
    return;
  }

protected:
  void start_workers() noexcept
  {
    std::lock_guard<std::mutex> lock { m_worker_count_mutex };
    std::lock_guard<std::recursive_mutex> queue_lock { m_queue_mutex };
    auto n_workers_to_start = std::min(m_max_worker_count - m_worker_count, m_queue.size());
    for (auto i = 0; i < n_workers_to_start; i++)
    {
      std::thread { &ThreadPool::worker, this }.detach();
    }
    m_worker_count += n_workers_to_start;
  }

  std::recursive_mutex m_queue_mutex;
  std::condition_variable m_queue_condition;
  std::deque<std::shared_ptr<IFuture>> m_queue;
  std::mutex m_worker_count_mutex;
  std::condition_variable m_worker_count_condition;
  std::size_t m_worker_count;
  std::size_t m_max_worker_count;
};
