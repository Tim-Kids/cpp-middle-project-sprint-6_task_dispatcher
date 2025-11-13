# TaskDispatcher Final Architecture & Implementation Roadmap

This roadmap describes the **final tuned architecture**, **APIs**, and **implementation plan** for the TaskDispatcher project — now aligned with both the course specification and the updated, optimized design.

---

# ✅ 1. Final Agreed APIs (After Tuning)

## `types.hpp`

```cpp
#pragma once

#include <functional>

namespace dispatcher {

enum class TaskPriority { High, Normal };

// Convenience alias for tasks
using Task = std::function<void()>;

}  // namespace dispatcher
```

---

## `queue/queue.hpp`

```cpp
#pragma once
#include <functional>
#include <optional>

#include "types.hpp"

namespace dispatcher::queue {

struct QueueOptions {
    bool bounded;
    std::optional<int> capacity;
};

class IQueue {
public:
    virtual ~IQueue() = default;
    virtual void push(dispatcher::Task task) = 0;
    virtual std::optional<dispatcher::Task> try_pop() = 0;
};

}  // namespace dispatcher::queue
```

---

## `queue/bounded_queue.hpp`

```cpp
#pragma once
#include "queue/queue.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>

namespace dispatcher::queue {

class BoundedQueue : public IQueue {
public:
    explicit BoundedQueue(int capacity);

    void push(dispatcher::Task task) override;
    std::optional<dispatcher::Task> try_pop() override;

    ~BoundedQueue() override;

private:
    std::queue<dispatcher::Task> queue_;
    int capacity_;
    std::mutex mtx_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
};

}  // namespace dispatcher::queue
```

---

## `queue/unbounded_queue.hpp`

```cpp
#pragma once
#include "queue/queue.hpp"

#include <condition_variable>
#include <mutex>
#include <queue>

namespace dispatcher::queue {

class UnboundedQueue : public IQueue {
public:
    UnboundedQueue() = default;

    void push(dispatcher::Task task) override;
    std::optional<dispatcher::Task> try_pop() override;

    ~UnboundedQueue() override = default;

private:
    std::queue<dispatcher::Task> queue_;
    std::mutex mtx_;
    std::condition_variable not_empty_;
};

}  // namespace dispatcher::queue
```

---

## `queue/priority_queue.hpp`

```cpp
#pragma once
#include "queue/bounded_queue.hpp"
#include "queue/unbounded_queue.hpp"
#include "queue/queue.hpp"
#include "types.hpp"

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace dispatcher::queue {

class PriorityQueue {
public:
    using Config = std::map<dispatcher::TaskPriority, QueueOptions>;

    explicit PriorityQueue(const Config& config);

    void push(dispatcher::TaskPriority priority, dispatcher::Task task);

    // Blocks until a task is available or shutdown() is called.
    std::optional<dispatcher::Task> pop();

    void shutdown();
    ~PriorityQueue();

private:
    std::map<dispatcher::TaskPriority, std::unique_ptr<IQueue>> queues_;
    bool active_{true};
    std::mutex mtx_;
    std::condition_variable cv_;
};

}  // namespace dispatcher::queue
```

---

## `thread_pool/thread_pool.hpp`

```cpp
#pragma once

#include "queue/priority_queue.hpp"

#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace dispatcher::thread_pool {

class ThreadPool {
public:
    ThreadPool(std::shared_ptr<dispatcher::queue::PriorityQueue> pq,
               std::size_t thread_count);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    void worker_loop();

    std::shared_ptr<dispatcher::queue::PriorityQueue> pq_;
    std::vector<std::thread> workers_;
};

} // namespace dispatcher::thread_pool
```

---

## `task_dispatcher.hpp`

```cpp
#pragma once

#include <functional>
#include <map>
#include <memory>

#include "queue/priority_queue.hpp"
#include "thread_pool/thread_pool.hpp"
#include "types.hpp"

namespace dispatcher {

class TaskDispatcher {
public:
    using QueueConfig = std::map<TaskPriority, queue::QueueOptions>;

    explicit TaskDispatcher(
        std::size_t thread_count,
        QueueConfig config = default_config()
    );

    void schedule(TaskPriority priority, Task task);

    ~TaskDispatcher();

    TaskDispatcher(const TaskDispatcher&) = delete;
    TaskDispatcher& operator=(const TaskDispatcher&) = delete;

private:
    static QueueConfig default_config();

    std::shared_ptr<queue::PriorityQueue> pq_;
    thread_pool::ThreadPool pool_;
};

}  // namespace dispatcher
```

---

# ✅ 2. Final Concurrency Model

### ✔ BoundedQueue
- `push()` blocks when full  
- `try_pop()` is non-blocking  
- uses mutex + 2 condition variables  

### ✔ UnboundedQueue
- `push()` never blocks  
- `try_pop()` non-blocking  
- simplest queue implementation  

### ✔ PriorityQueue (The Heart)
- Only place where **blocking pop()** happens  
- `push()` forwards to the right internal queue  
- `pop()` logic:
  1. Try HIGH
  2. Try NORMAL
  3. If nothing & active → wait
  4. If shutdown → return nullopt

### ✔ ThreadPool
- Worker loop calls `pop()` until nullopt  
- Executes tasks with try/catch  
- Destructor joins all worker threads  

### ✔ TaskDispatcher
- Builds PQ + ThreadPool  
- Default config:
  - High → bounded(1000)  
  - Normal → unbounded  
- `schedule()` forwards tasks into PQ  
- Destructor:
  - calls `pq_->shutdown()`  
  - ThreadPool destructor joins threads  

---

# ✅ 3. Final Implementation Steps

## **Step 1 — Implement BoundedQueue**
- queue + mutex + CVs  
- `push()` blocks when full  
- `try_pop()` non-blocking  

## **Step 2 — Implement UnboundedQueue**
- identical but no capacity checks  

## **Step 3 — Implement PriorityQueue**
- constructor creates correct queue types  
- `push()` → route + notify  
- `pop()` → multi-priority check + wait  
- `shutdown()` → set active=false, notify_all  

## **Step 4 — Implement ThreadPool**
- start N workers  
- worker_loop: pop → exec → repeat  
- destructor joins all threads  

## **Step 5 — Implement TaskDispatcher**
- default config  
- schedule() forwards  
- shutdown on destroy  

## **Step 6 — Unit Tests**
- BoundedQueue tests  
- UnboundedQueue tests  
- PriorityQueue tests  
- TaskDispatcher tests  

---

# 🎉 Final Result

A complete C++ concurrency system featuring:

- Priority-based task scheduling  
- Blocking + non-blocking queues  
- Shared PriorityQueue coordination  
- Worker thread pool  
- Graceful shutdown  
- Modern C++ idioms (RAII, lambdas, optional, threading primitives)

Fully compatible with the provided `main.cpp` logging demo.

