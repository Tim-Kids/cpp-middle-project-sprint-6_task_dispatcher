#pragma once

#include <memory>

#include "queue/priority_queue.hpp"
#include "thread_pool/thread_pool.hpp"
#include "types.hpp"

namespace dispatcher {

inline const std::map<TaskPriority, queue::QueueOptions> init_config = {
    {TaskPriority::High, queue::QueueOptions {true, 1000}},
    {TaskPriority::Normal, queue::QueueOptions {false, std::nullopt}}};

class TaskDispatcher {
    std::shared_ptr<queue::PriorityQueue> pq_    = nullptr;
    std::unique_ptr<thread_pool::ThreadPool> tp_ = nullptr;

    public:
    explicit TaskDispatcher(size_t thread_count,
                            const std::map<TaskPriority, queue::QueueOptions>& config = init_config);

    void Schedule(TaskPriority priority, std::function<void()> task);
};

}  // namespace dispatcher
