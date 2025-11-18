#include "queue/bounded_queue.hpp"

namespace dispatcher::queue {

BoundedQueue::BoundedQueue(int capacity): capacity_(capacity) {}

void BoundedQueue::Push(std::function<void()> task) {
    std::unique_lock lock(mutex_);
    not_full_.wait(lock, [&] { return queue_.size() < capacity_; });
    queue_.push(std::move(task));
    lock.unlock();
    not_empty_.notify_one();
}

std::optional<std::function<void()>> BoundedQueue::Pop() {
    std::unique_lock lock(mutex_);
    not_empty_.wait(lock, [&] { return !queue_.empty(); });
    auto task = std::move(queue_.front());
    queue_.pop();
    lock.unlock();
    not_full_.notify_one();
    return task;
}

std::optional<std::function<void()>> BoundedQueue::TryPop() {
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

}  // namespace dispatcher::queue
