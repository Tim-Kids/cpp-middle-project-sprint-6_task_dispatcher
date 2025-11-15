#pragma once

#include "queue/queue.hpp"

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>

namespace dispatcher::queue {

class BoundedQueue: public IQueue {
    std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    size_t capacity_;
    std::queue<Task> queue_;

    public:
    explicit BoundedQueue(int capacity);

    void Push(Task task) override;

    [[nodiscard]] std::optional<Task> TryPop() override;

    std::optional<Task> Pop() override;

    ~BoundedQueue() override = default;
};

}  // namespace dispatcher::queue
