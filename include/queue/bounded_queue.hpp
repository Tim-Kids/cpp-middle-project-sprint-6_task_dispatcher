#pragma once

#include "queue/queue.hpp"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

namespace dispatcher::queue {

class BoundedQueue: public IQueue {
    std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    size_t capacity_;
    std::queue<std::function<void()>> queue_;

    public:
    explicit BoundedQueue(int capacity);

    void Push(std::function<void()> task) override;

    std::optional<std::function<void()>> TryPop() override;

    std::optional<std::function<void()>> Pop() override;
};

}  // namespace dispatcher::queue
