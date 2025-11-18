#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <chrono>

#include "queue/unbounded_queue.hpp"

#include <numeric>

using namespace dispatcher;
using namespace dispatcher::queue;

TEST(UnboundedQueueTest, PushPopBasicFIFO) {
    UnboundedQueue q;

    q.Push([] {});
    q.Push([] {});
    q.Push([] {});

    auto t1 = q.Pop();
    auto t2 = q.Pop();
    auto t3 = q.Pop();

    EXPECT_TRUE(t1.has_value());
    EXPECT_TRUE(t2.has_value());
    EXPECT_TRUE(t3.has_value());
}

TEST(UnboundedQueueTest, TryPopEmptyReturnsNullopt) {
    UnboundedQueue q;

    auto v = q.TryPop();
    EXPECT_FALSE(v.has_value());
}

TEST(UnboundedQueueTest, TryPopNonBlocking) {
    UnboundedQueue q;

    ASSERT_FALSE(q.TryPop().has_value());

    q.Push([] {});
    ASSERT_TRUE(q.TryPop().has_value());
}

TEST(UnboundedQueueTest, PopBlocksUntilItemAvailable) {
    UnboundedQueue q;

    auto fut = std::async(std::launch::async, [&] {
        auto val = q.Pop();
        return val.has_value();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ASSERT_EQ(fut.wait_for(std::chrono::milliseconds(0)), std::future_status::timeout);

    q.Push([] {});

    ASSERT_EQ(fut.wait_for(std::chrono::milliseconds(200)), std::future_status::ready);
    ASSERT_TRUE(fut.get());
}

TEST(UnboundedQueueTest, PopDoesNotMissNotifications) {
    UnboundedQueue q;

    auto fut = std::async(std::launch::async, [&] {
        auto t1 = q.Pop();
        auto t2 = q.Pop();
        return t1.has_value() && t2.has_value();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    q.Push([] {});
    q.Push([] {});

    ASSERT_EQ(fut.wait_for(std::chrono::milliseconds(300)), std::future_status::ready);
    ASSERT_TRUE(fut.get());
}

TEST(UnboundedQueueTest, MultiProducerMultiConsumerStress) {
    UnboundedQueue q;
    static constexpr int N = 200;

    std::atomic<int> counter = 0;

    auto producer = [&q] {
        for(int i: std::ranges::iota_view(0, N)) {
            q.Push([] {});
        }
    };

    auto consumer = [&] {
        for(int i: std::ranges::iota_view(0, N)) {
            auto task = q.Pop();
            ASSERT_TRUE(task.has_value());
            (*task)();
            counter++;
        }
    };

    {
        std::jthread p1(producer);
        std::jthread p2(producer);
        std::jthread c1(consumer);
        std::jthread c2(consumer);
    }

    ASSERT_EQ(counter.load(), 2 * N);
}
