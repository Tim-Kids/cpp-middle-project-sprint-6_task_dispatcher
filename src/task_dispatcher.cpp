#include "task_dispatcher.hpp"

namespace dispatcher {

TaskDispatcher::TaskDispatcher(size_t thread_count, const std::map<TaskPriority, queue::QueueOptions>& config):
    pq_(std::make_shared<queue::PriorityQueue>(config)),
    tp_(std::make_unique<thread_pool::ThreadPool>(pq_, thread_count)) {}

void TaskDispatcher::Schedule(TaskPriority priority, std::function<void()> task) {
    pq_->Push(priority, std::move(task));
}

}  // namespace dispatcher
