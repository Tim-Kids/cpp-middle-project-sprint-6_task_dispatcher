#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>
#include <mutex>

#include "task_dispatcher.hpp"
#include "queue/priority_queue.hpp"
#include "types.hpp"

using dispatcher::TaskDispatcher;
using dispatcher::TaskPriority;
using dispatcher::queue::QueueOptions;

const std::map<TaskPriority, QueueOptions> config = {{TaskPriority::High, QueueOptions {true, 100}},
                                                     {TaskPriority::Normal, QueueOptions {false, std::nullopt}}};

constexpr auto SHORT = std::chrono::milliseconds(50);
constexpr auto LONG  = std::chrono::milliseconds(200);

TEST(TaskDispatcherTest, ExecutesScheduledTasks) {
    std::atomic<int> counter = 0;

    {
        TaskDispatcher td(std::thread::hardware_concurrency(), config);

        for(int i = 0; i < 50; ++i) {
            td.Schedule(TaskPriority::Normal, [&] { counter++; });
        }
    }

    ASSERT_EQ(counter.load(), 50);
}

TEST(TaskDispatcherTest, MultiThreadProducers) {
    std::atomic<int> counter = 0;

    {
        TaskDispatcher td(4, config);

        std::vector<std::jthread> producers;

        for(int t = 0; t < 5; ++t) {
            producers.emplace_back([&] {
                for(int i = 0; i < 40; ++i) {
                    td.Schedule(TaskPriority::Normal, [&] { counter++; });
                }
            });
        }
    }

    ASSERT_EQ(counter.load(), 200);
}

TEST(TaskDispatcherTest, ExceptionsInsideTasksHandled) {
    std::atomic<int> ok = 0;

    {
        TaskDispatcher td(4, config);

        td.Schedule(TaskPriority::Normal, [] { throw std::runtime_error("boom"); });

        td.Schedule(TaskPriority::Normal, [] { throw 123; });

        td.Schedule(TaskPriority::Normal, [&] { ok++; });
    }

    ASSERT_EQ(ok.load(), 1);
}

TEST(TaskDispatcherTest, StressInterleavingHighAndNormal) {
    std::atomic<int> normal_count = 0;
    std::atomic<int> high_count   = 0;

    {
        TaskDispatcher td(8, config);

        std::vector<std::jthread> threads;

        for(int t = 0; t < 4; ++t) {
            threads.emplace_back([&] {
                for(int i = 0; i < 200; ++i) {
                    td.Schedule(TaskPriority::High, [&] { high_count++; });
                    td.Schedule(TaskPriority::Normal, [&] { normal_count++; });
                }
            });
        }
    }

    ASSERT_EQ(high_count.load(), 800);
    ASSERT_EQ(normal_count.load(), 800);
}

TEST(TaskDispatcherTest, NoTaskLossOnConcurrentShutdown) {
    std::atomic<int> counter = 0;

    TaskDispatcher* dispatcher = new TaskDispatcher(4, config);

    for(int i = 0; i < 100; ++i) {
        dispatcher->Schedule(TaskPriority::Normal, [&] { counter++; });
    }

    {
        // Удаление диспетчера даже из другого потока должно проходить корректно - воркеры должны завершить все задачи.
        std::jthread killer([&] { delete dispatcher; });
    }

    ASSERT_EQ(counter.load(), 100);
}
