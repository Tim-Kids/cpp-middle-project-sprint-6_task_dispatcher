#include "queue/priority_queue.hpp"

#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <chrono>

#include "queue/bounded_queue.hpp"
#include "queue/unbounded_queue.hpp"
#include "types.hpp"

using namespace dispatcher;
using namespace dispatcher::queue;

constexpr auto SHORT = std::chrono::milliseconds(50);
constexpr auto LONG  = std::chrono::milliseconds(200);

struct MyPriorityQueueTest: public testing::Test {
    const std::map<TaskPriority, QueueOptions> config = {{TaskPriority::High, QueueOptions {true, 100}},
                                                         {TaskPriority::Normal, QueueOptions {false, std::nullopt}}};

    std::unique_ptr<PriorityQueue> pq = nullptr;

    void SetUp() override {
        pq = std::make_unique<PriorityQueue>(config);
    }
};

TEST_F(MyPriorityQueueTest, ConstructQueuesFromConfig) {

    // Проверка конструктора.
    ASSERT_EQ(pq->GetQueues().size(), 2);

    // Одна очередь - ограниченная.
    auto high = pq->GetQueues().find(TaskPriority::High);
    ASSERT_NE(high, pq->GetQueues().end());
    // Should be a BoundedQueue
    ASSERT_NE(dynamic_cast<BoundedQueue*>(high->second.get()), nullptr);

    // Другая - ограниченная.
    auto normal = pq->GetQueues().find(TaskPriority::Normal);
    ASSERT_NE(normal, pq->GetQueues().end());
    ASSERT_NE(dynamic_cast<UnboundedQueue*>(normal->second.get()), nullptr);
}

TEST_F(MyPriorityQueueTest, SingleTaskPushPop) {

    std::atomic<int> val = 0;
    pq->Push(TaskPriority::Normal, [&] { val = 123; });

    auto task = pq->Pop();
    ASSERT_TRUE(task.has_value());
    (*task)();

    ASSERT_EQ(val.load(), 123);
}

TEST_F(MyPriorityQueueTest, HighPriorityServedBeforeNormal) {
    std::vector<std::string> order;

    pq->Push(TaskPriority::Normal, [&] { order.push_back("N1"); });
    pq->Push(TaskPriority::Normal, [&] { order.push_back("N2"); });

    pq->Push(TaskPriority::High, [&] { order.push_back("H1"); });
    pq->Push(TaskPriority::High, [&] { order.push_back("H2"); });

    for(int i = 0; i < 4; ++i) {
        auto task = pq->Pop();
        ASSERT_TRUE(task.has_value());
        (*task)();
    }

    ASSERT_EQ(order.size(), 4);

    ASSERT_EQ(order[0], "H1");
    ASSERT_EQ(order[1], "H2");
    ASSERT_EQ(order[2], "N1");
    ASSERT_EQ(order[3], "N2");
}

TEST_F(MyPriorityQueueTest, PopBlocksUntilTaskArrives) {

    auto fut = std::async(std::launch::async, [&]() {
        auto t = pq->Pop();  // Поток fut блокируется, потому что очередь пустая.
        return t.has_value();
    });

    std::this_thread::sleep_for(SHORT);
    ASSERT_EQ(fut.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);

    pq->Push(TaskPriority::Normal, [] {});  // После пуша поток fut будет разблокирован.

    // Ждем какое-то время и проверяем готовность потока fut - должна быть true.
    ASSERT_EQ(fut.wait_for(LONG), std::future_status::ready);
    ASSERT_TRUE(fut.get());
}

TEST_F(MyPriorityQueueTest, ShutdownCausesPopToReturnNullopt) {

    auto f = std::async(std::launch::async, [&] {
        auto t = pq->Pop();  // Поток блокируется, т.к. очередь пустая.
        return t;  // 1 - fut попробует изъять задачу из очереди. После неуспешной попытки (очередь все еще пуста)
                   // увидит команду Shutdown и вернет nullopt.
    });

    std::this_thread::sleep_for(SHORT);
    pq->Shutdown();  // 2 - разблокирует fut. Управление перейдет в fut (1).

    auto r = f.get();
    ASSERT_FALSE(r.has_value());
}

TEST_F(MyPriorityQueueTest, TasksQueuedBeforeShutdownAreDrained) {
    std::atomic<int> counter = 0;

    for(int i = 0; i < 5; ++i) {
        pq->Push(TaskPriority::Normal, [&] { counter++; });
    }

    pq->Shutdown();

    // Обрабатываем задачи в одном потоке вручную.
    while(true) {
        auto task = pq->Pop();
        if(!task) {
            break;
        }
        (*task)();
    }

    ASSERT_EQ(counter.load(), 5);
}

TEST_F(MyPriorityQueueTest, MultiThreadedStressTest) {

    std::atomic<int> executed = 0;

    constexpr int HIGH_COUNT = 50;
    constexpr int NORM_COUNT = 50;

    auto prod_high = [&] {
        for(int i = 0; i < HIGH_COUNT; ++i) {
            pq->Push(TaskPriority::High, [&] { executed++; });
        }
    };

    auto prod_norm = [&] {
        for(int i = 0; i < NORM_COUNT; ++i) {
            pq->Push(TaskPriority::Normal, [&] { executed++; });
        }
    };

    auto consumer = [&] {
        while(true) {
            auto task = pq->Pop();
            if(!task) {
                break;
            }
            (*task)();
        }
    };

    std::thread t1(prod_high);
    std::thread t2(prod_norm);

    std::thread c1(consumer);
    std::thread c2(consumer);

    t1.join();
    t2.join();

    pq->Shutdown();

    c1.join();
    c2.join();

    ASSERT_EQ(executed.load(), HIGH_COUNT + NORM_COUNT);
}

TEST_F(MyPriorityQueueTest, PushThrowsForMissingPriorityQueue) {
    const std::map<TaskPriority, QueueOptions> config = {{TaskPriority::High, QueueOptions {true, 10}}};

    PriorityQueue pq(config);

    ASSERT_THROW(pq.Push(TaskPriority::Normal, [] {}), std::invalid_argument);
}

TEST_F(MyPriorityQueueTest, PopAfterShutdownAndEmptyQueuesReturnsNulloptImmediately) {
    pq->Shutdown();

    auto task = pq->Pop();
    ASSERT_FALSE(task.has_value());
}
