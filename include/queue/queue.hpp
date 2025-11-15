#pragma once

#include "priority_queue.hpp"

#include <functional>
#include <optional>

namespace dispatcher::queue {

using Task = std::function<void()>;

struct QueueOptions {
    bool bounded;
    std::optional<int> capacity;
};

class IQueue {
public:
    virtual ~IQueue() = default;
    virtual void Push(Task task) = 0;
    virtual std::optional<Task> TryPop() = 0;
    virtual std::optional<Task> Pop() = 0;
};

}  // namespace dispatcher::queue