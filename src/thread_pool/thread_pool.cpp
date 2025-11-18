#include "thread_pool/thread_pool.hpp"

#include <numeric>
#include <algorithm>
#include <print>
#include <print>

namespace dispatcher::thread_pool {

ThreadPool::ThreadPool(std::shared_ptr<queue::PriorityQueue> pq, size_t num_threads): pq_(pq) {
    workers_.reserve(num_threads);
    for(int i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&ThreadPool::Run, this);
    }
}

ThreadPool::~ThreadPool() {
    // После вызова деструктора потоки должны исполнять задачи, пока из приоритетной очереди не вернется std::nullopt.
    if(pq_) {
        pq_->Shutdown();
    }
    for(auto& worker: workers_) {
        if(worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::Run() {
    while(true) {
        auto task = pq_->Pop();  // NVRO
        if(!task) {
            return;  // Прекращаем работу после того, как получили команду Shutdown().
        }
        // Так как задачи независимы, то нет смысла использовать примитивы синхронизации при выполнении задач.
        try {
            (*task)();
        }
        catch(const std::exception& e) {
            std::println("Exception thrown while running task: {}", e.what());
        }
        catch(...) {
            std::println("Unknown exception thrown while running task");
        }
    }
}

}  // namespace dispatcher::thread_pool
