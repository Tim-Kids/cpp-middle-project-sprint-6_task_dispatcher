#pragma once

#include "queue/priority_queue.hpp"

#include <thread>
#include <atomic>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>

namespace dispatcher::thread_pool {

class ThreadPool {
  std::shared_ptr<queue::PriorityQueue> pq_ = nullptr;
  std::vector<std::jthread> workers_ {};

  public:
  ThreadPool(std::shared_ptr<queue::PriorityQueue> pq, size_t num_threads = std::thread::hardware_concurrency());

  ~ThreadPool();

  private:
  void Run();

};

} // namespace dispatcher::thread_pool
