#pragma once

#include "queue/queue.hpp"

#include <condition_variable>
#include <functional>
#include <optional>
#include <mutex>
#include <queue>

namespace dispatcher::queue {

class UnboundedQueue: public IQueue {
    std::queue<std::function<void()>> queue_;
    std::condition_variable not_empty_;
    std::mutex mutex_;

    public:
    UnboundedQueue() = default;

    void Push(std::function<void()> task) override;

    std::optional<std::function<void()>> Pop() override;
    std::optional<std::function<void()>> TryPop() override;
};

}  // namespace dispatcher::queue
