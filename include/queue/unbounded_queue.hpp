#pragma once

#include "queue/queue.hpp"

#include <functional>
#include <optional>

namespace dispatcher::queue {

/*class UnboundedQueue : public IQueue {
    // здесь ваш код
public:
    explicit UnboundedQueue(int capacity);

    void Push(std::function<void()> task) override;

    std::optional<std::function<void()>> Pop() override;
    std::optional<std::function<void()>> TryPop() override;

    ~UnboundedQueue() override;
};*/

}  // namespace dispatcher::queue