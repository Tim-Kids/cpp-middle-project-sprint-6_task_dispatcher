#include "queue/unbounded_queue.hpp"

namespace dispatcher::queue {

void UnboundedQueue::Push(std::function<void()>  task) {
    std::lock_guard lock(mutex_);
    queue_.push(std::move(task));
    not_empty_.notify_one();
}

std::optional<std::function<void()>> UnboundedQueue::Pop() {
    std::unique_lock lock(mutex_);
    not_empty_.wait(lock, [&] { return !queue_.empty(); });
    auto task = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    return task;
}

std::optional<std::function<void()>> UnboundedQueue::TryPop() {
    mutex_.lock();
    if(queue_.empty()) {
        mutex_.unlock();
        return std::nullopt;
    }
    auto task = std::move(queue_.front());
    queue_.pop();
    mutex_.unlock();
    return task;
}


} // namespace dispatcher::queue