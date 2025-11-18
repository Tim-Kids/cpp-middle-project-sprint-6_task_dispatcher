#include "queue/priority_queue.hpp"

#include <exception>
#include <algorithm>
#include <memory>

namespace dispatcher::queue {

PriorityQueue::PriorityQueue(const std::map<TaskPriority, QueueOptions>& config) {
    for(const auto& [priority, options]: config) {
        if(options.bounded) {
            if(!options.capacity) {
                throw std::invalid_argument("Bounded priority queue can't be based on zero capacity");
            }
            priority_queues_.try_emplace(priority, std::make_unique<BoundedQueue>(options.capacity.value()));
        }
        else {
            priority_queues_.try_emplace(priority, std::make_unique<UnboundedQueue>());
        }
    }
}

void PriorityQueue::Push(TaskPriority priority, std::function<void()> task) {
    std::unique_ptr<IQueue>* q_ptr = nullptr;
    {
        std::lock_guard guard(mutex_);
        if(auto queue_it = priority_queues_.find(priority); queue_it != priority_queues_.end()) {
            q_ptr = &queue_it->second;
        }
        else {
            throw std::invalid_argument("Priority queue does not exist");
        }
    }

    // В нашем случае дедлока не произойдет, если вызвать Push под мьютексом PriorityQueue, потому что логика
    // приложения подразумевает строгий порядок захвата мьютексов потоками: 1. Мьютекс из PriorityQueue 2. Мьютекс
    // подлежащей Bounded/UnboundedQueue. Однако то, что мы вынесли Push после разблокировки мьютекса немного повысит
    // производительность при больших нагрузках.
    (*q_ptr)->Push(std::move(task));
    cv_.notify_one();
}

std::optional<std::function<void()>> PriorityQueue::Pop() {
    std::unique_lock lock(mutex_);

    while(true) {  // Просыпаемся и проверяем, что не было каманды Shutdown(), а очередь все еще активна. При этом
                   // active_ должен менять свое состояние (другим потоком) только под тем же мьютексом. Because
                   // cv_.wait(lock) only synchronizes visibility of writes that happened before the mutex was
                   // unlocked in the notifying thread.
        if(auto high = priority_queues_.find(TaskPriority::High); high != priority_queues_.cend()) {
            if(auto task = high->second->TryPop()) {  // Сперва проверяем задачи с высоким приоритетом.
                lock.unlock();  // Пусть потоки проснуться чуть раньше и смогут снова выполнять полезную работу.
                return task;
            }
        }
        if(auto normal = priority_queues_.find(TaskPriority::Normal); normal != priority_queues_.cend()) {
            if(auto task = normal->second->TryPop()) {  // Только потом проверяем задачи с нормальным приоритетом.
                lock.unlock();
                return task;
            }
        }

        if(!active_) {
            return std::nullopt;  // Получили команду Shutdown(). В этой точке все задачи,
                                  // которые взяли себе потоки в Pop(), гарантированно завершены.
        }

        cv_.wait(lock);  // Засыпаем и отпускаем мьютекс.
    }
}

void PriorityQueue::Shutdown() {
    {
        std::lock_guard lock(mutex_);  // Синхронизируемся обязательно под тем же мьютексом, что и cv_ в Pop(). Только
                                       // так код внутри cv_ увидит актулаьные значения разделяемых данных.
        active_ = false;
    }
    cv_.notify_all();  // Пробуждаем в Pop() все спящие потоки - корректно завершаем работу.
}

}  // namespace dispatcher::queue
