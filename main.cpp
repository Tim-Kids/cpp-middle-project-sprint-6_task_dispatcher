#include <chrono>
#include <thread>
#include <print>

#include "logger.hpp"
#include "task_dispatcher.hpp"

using namespace dispatcher;

int main() {

    std::println("Task dispatcher started.\n");
    {
        TaskDispatcher td(
            std::thread::hardware_concurrency());  // Объект диспетчера. Его воркеры делают всю тяжелую работу.
        std::vector<std::jthread>
            threads;  // Лекговесные потоки, которые только кладут задачи в диспетчер и засыпают в ОС.
        threads.reserve(5);

        for(int i = 0; i < 5; ++i) {
            threads.emplace_back([&, i]() {
                for(int j = 0; j < 10; j++) {
                    td.Schedule(TaskPriority::Normal,
                                [=]() { Logger::Get().Log("Normal priority message #" + std::to_string(10 * i + j)); });
                    td.Schedule(TaskPriority::High,
                                [=]() { Logger::Get().Log("High priority message #" + std::to_string(10 * i + j)); });
                }
            });
        }

        // Последним при расрутки стека вызовется деструктор TaskDispatcher. Воркеры сделают все работу и корректно
        // освободят ресурсы.
    }

    std::println("\nTask dispatcher completed.");
}
