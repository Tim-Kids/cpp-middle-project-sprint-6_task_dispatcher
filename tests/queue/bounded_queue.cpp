#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <chrono>
#include <ranges>
#include <algorithm>

#include "queue/bounded_queue.hpp"

TEST(BasicCheck, SimpleCheck) {
    EXPECT_EQ(1 + 1, 2);
}

using namespace dispatcher;
using namespace dispatcher::queue;

TEST(BoundedQueueTest, PushPopBasicFIFO) {
    BoundedQueue q(3);

    q.Push([] {});
    q.Push([] {});
    q.Push([] {});

    auto t1 = q.Pop();
    auto t2 = q.Pop();
    auto t3 = q.Pop();

    ASSERT_TRUE(t1.has_value());
    ASSERT_TRUE(t2.has_value());
    ASSERT_TRUE(t3.has_value());
}

TEST(BoundedQueueTest, TryPopReturnsEmptyWhenQueueEmpty) {
    BoundedQueue q(2);

    auto t = q.TryPop();
    ASSERT_FALSE(t.has_value());
}

TEST(BoundedQueueTest, PushBlocksWhenFull) {
    BoundedQueue q(1);

    // Заполняем очередь.
    q.Push([] {});

    // Пытаемся запушить задачу из другого потока.
    auto fut = std::async(std::launch::async, [&] {
        q.Push([] {});
        return true;  // Эта строчка сработает, если текущий поток fut проснется в Push().
    });

    // Проверяем, что поток fut все еще заблокирован и не завершил задачу.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(fut.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);

    // Разблокируем поток fut.
    auto t = q.Pop();
    ASSERT_TRUE(t.has_value());

    // Теперь поток fut может запушить новую задачу.
    ASSERT_EQ(fut.wait_for(std::chrono::milliseconds(200)), std::future_status::ready);
}

TEST(BoundedQueueTest, PopBlocksUntilItemArrives) {
    BoundedQueue q(2);

    auto fut = std::async(std::launch::async, [&] {
        auto val = q.Pop();
        return val.has_value();
    });

    // Проверяем, что поток fut все еще заблокирован и не завершил задачу.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(fut.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);

    // Пушим задачу в очередь для того, чтобы fut разблокировался в Push().
    q.Push([] {});

    // Теперь поток fut может забрать новую задачу.
    ASSERT_EQ(fut.wait_for(std::chrono::milliseconds(200)), std::future_status::ready);
    ASSERT_TRUE(fut.get());
}

TEST(BoundedQueueTest, TryPopNonBlocking) {
    BoundedQueue q(2);

    ASSERT_FALSE(q.TryPop().has_value());

    q.Push([] {});
    ASSERT_TRUE(q.TryPop().has_value());
}

TEST(BoundedQueueTest, MultiProducerMultiConsumer) {
    BoundedQueue q(10);

    std::atomic<int> counter = 0;

    auto producer = [&] {
        for(int i: std::ranges::iota_view(0, 50)) {
            q.Push([&] { counter++; });
        }
    };

    auto consumer = [&] {
        for(int i: std::ranges::iota_view(0, 50)) {
            auto task = q.Pop();
            ASSERT_TRUE(task.has_value());
            (*task)();
        }
    };

    {
        std::jthread p1(producer);
        std::jthread p2(producer);
        std::jthread c1(consumer);
        std::jthread c2(consumer);
    }

    EXPECT_EQ(counter.load(), 100);
}
