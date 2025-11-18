#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "thread_pool/thread_pool.hpp"
#include "queue/priority_queue.hpp"
#include "types.hpp"

using dispatcher::TaskPriority;
using dispatcher::queue::PriorityQueue;
using dispatcher::queue::QueueOptions;
using dispatcher::thread_pool::ThreadPool;

struct MyThreadPoolTest: public testing::Test {
    const std::map<TaskPriority, QueueOptions> config = {{TaskPriority::High, QueueOptions {true, 100}},
                                                         {TaskPriority::Normal, QueueOptions {false, std::nullopt}}};

    std::shared_ptr<PriorityQueue> pq = nullptr;

    void SetUp() override {
        pq = std::make_shared<PriorityQueue>(config);
    }
};

constexpr auto SHORT = std::chrono::milliseconds(50);
constexpr auto LONG  = std::chrono::milliseconds(200);

TEST_F(MyThreadPoolTest, ExecutesAllTasks) {
    std::atomic<int> counter = 0;

    {
        ThreadPool pool(pq, 4);
        for(int i = 0; i < 100; ++i) {
            pq->Push(TaskPriority::Normal, [&] { counter.fetch_add(1); });
        }
    }  // Воркеры гарантированно завершат работу при вызове дестркутора тредпула.

    ASSERT_EQ(counter.load(), 100);
}

TEST_F(MyThreadPoolTest, WorkersBlockAndWakeCorrectly) {
    std::atomic<int> val = 0;

    {
        ThreadPool pool(pq, 2);
        // Имитируем блокировку воркеров.
        std::this_thread::sleep_for(SHORT);

        pq->Push(TaskPriority::Normal, [&] { val.store(1, std::memory_order_release); });

        std::this_thread::sleep_for(LONG);
    }

    ASSERT_EQ(val.load(), 1);
}

TEST_F(MyThreadPoolTest, ShutdownStopsWorkers) {
    auto pool = std::make_unique<ThreadPool>(pq, 4);

    std::this_thread::sleep_for(SHORT);

    pool.reset();  // Вызывает Shutdown + join у всех воркеров.

    auto t = pq->Pop();  // Воркеры гарантированно завершили все задачи, очередь пуста, вернется std::nullopt.
    ASSERT_FALSE(t.has_value());
}

TEST_F(MyThreadPoolTest, DrainsTasksBeforeExit) {
    std::atomic<int> counter = 0;

    {
        ThreadPool pool(pq, 3);
        for(int i = 0; i < 20; ++i) {
            pq->Push(TaskPriority::Normal, [&] { counter++; });
        }
    }

    ASSERT_EQ(counter.load(), 20);
}

TEST_F(MyThreadPoolTest, RespectsPriorities) {
    std::vector<std::string> order;

    pq->Push(TaskPriority::Normal, [&] { order.emplace_back("N1"); });
    pq->Push(TaskPriority::Normal, [&] { order.emplace_back("N2"); });
    pq->Push(TaskPriority::High, [&] { order.emplace_back("H1"); });
    pq->Push(TaskPriority::High, [&] { order.emplace_back("H2"); });

    {
        ThreadPool pool(pq, 3);  // Сами воркеры не гарантируют приоритетность задач. Если в очереди нет задач с
                                 // High-приоритетом, то воркер возьмет в работу Noraml и не будет ждать поступления
                                 // задач с высоким приоритетом. Поэтому спрева заполняем очеред в (1), потом запускаем
                                 // воркеров в отдельном скоупе (воркеры вызывают Pop() в своем деструкторе).
    }

    std::this_thread::sleep_for(LONG);

    ASSERT_EQ(order.size(), 4);
    ASSERT_EQ(order[0], "H1");
    ASSERT_EQ(order[1], "H2");
    ASSERT_EQ(order[2], "N1");
    ASSERT_EQ(order[3], "N2");
}

TEST_F(MyThreadPoolTest, MultiThreadedProducers) {

    std::atomic<int> count = 0;

    auto producer = [&](int n) {
        for(int i = 0; i < n; ++i) {
            pq->Push(TaskPriority::Normal, [&] { count++; });
        }
    };

    {
        std::jthread p1(producer, 50);
        std::jthread p2(producer, 50);
    }

    {
        ThreadPool pool(pq, 4);
    }

    std::this_thread::sleep_for(LONG);

    ASSERT_EQ(count.load(), 100);
}

TEST_F(MyThreadPoolTest, StdExceptionInsideTaskHandled) {
    std::atomic<int> ok = 0;

    // Бросает исключение в таске и обрабатывает его.
    pq->Push(TaskPriority::Normal, [] { throw std::runtime_error("boom"); });

    //  Дальше продолжает корректную работу.
    pq->Push(TaskPriority::Normal, [&] { ok++; });

    {
        ThreadPool pool(pq, 2);
    }

    std::this_thread::sleep_for(LONG);

    ASSERT_EQ(ok.load(), 1);
}

TEST_F(MyThreadPoolTest, NonStdExceptionInsideTaskHandled) {
    std::atomic<int> ok = 0;

    // Бросаем не std-исключение. Также должен не нарушать работу воркеров.
    pq->Push(TaskPriority::Normal, [] { throw 123; });

    // Продолжаем корректно работать.
    pq->Push(TaskPriority::Normal, [&] { ok++; });

    {
        ThreadPool pool(pq, 2);
    }

    std::this_thread::sleep_for(LONG);

    EXPECT_EQ(ok.load(), 1);
}
