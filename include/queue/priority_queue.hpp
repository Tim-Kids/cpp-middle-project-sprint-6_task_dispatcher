#pragma once
#include "queue/bounded_queue.hpp"
#include "queue/unbounded_queue.hpp"
#include "types.hpp"

#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace dispatcher::queue {

class PriorityQueue {
    std::map<TaskPriority, std::unique_ptr<IQueue>> priority_queues_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool active_{true};
public:
    explicit PriorityQueue(const std::map<TaskPriority, QueueOptions>& config);

    void Push(TaskPriority priority, std::function<void()> task);

    std::optional<std::function<void()>> Pop();

    void Shutdown();

    // Для юнит-тестирования класса.
    auto& GetQueues() const {
        return priority_queues_;
    }

    ~PriorityQueue() = default;
};

}  // namespace dispatcher::queue