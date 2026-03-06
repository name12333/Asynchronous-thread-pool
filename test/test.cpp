#include "../include/ThreadPool.h"
#include <iostream>
#include <random>
#include <chrono>

using namespace threadpool;

// 模拟耗时任务
void simulateWork(int id, int duration_ms) {
    std::cout << "Task " << id << " started, will take " << duration_ms << " ms" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    std::cout << "Task " << id << " completed" << std::endl;
}

// 计算斐波那契数列
int fibonacci(int n) {
    if (n <= 1) return n;
    return fibonacci(n - 1) + fibonacci(n - 2);
}

int main() {
    // 创建线程池，最小2个线程，最大8个线程
    ThreadPoolCore pool(2, 8, 60000, true);

    // 设置任务前钩子
    pool.setPreTaskHook([](uint64_t task_id) {
        std::cout << "Task " << task_id << " is about to start" << std::endl;
    });

    // 设置任务后钩子
    pool.setPostTaskHook([](uint64_t task_id, TaskStatus status) {
        std::cout << "Task " << task_id << " finished with status: ";
        switch (status) {
            case TaskStatus::PENDING:
                std::cout << "PENDING";
                break;
            case TaskStatus::RUNNING:
                std::cout << "RUNNING";
                break;
            case TaskStatus::COMPLETED:
                std::cout << "COMPLETED";
                break;
            case TaskStatus::CANCELLED:
                std::cout << "CANCELLED";
                break;
            case TaskStatus::TIMEOUT:
                std::cout << "TIMEOUT";
                break;
            case TaskStatus::FAILED:
                std::cout << "FAILED";
                break;
        }
        std::cout << std::endl;
    });

    // 启动线程池
    pool.start();

    // 添加不同优先级的任务
    std::vector<std::future<void>> futures;

    // 添加低优先级任务
    for (int i = 0; i < 5; ++i) {
        auto future = pool.addTask(TaskPriority::LOW, simulateWork, i, 500);
        futures.push_back(std::move(future));
    }

    // 添加普通优先级任务
    for (int i = 5; i < 10; ++i) {
        auto future = pool.addTask(TaskPriority::NORMAL, simulateWork, i, 300);
        futures.push_back(std::move(future));
    }

    // 添加高优先级任务
    for (int i = 10; i < 15; ++i) {
        auto future = pool.addTask(TaskPriority::HIGH, simulateWork, i, 200);
        futures.push_back(std::move(future));
    }

    // 添加关键优先级任务（计算斐波那契数列）
    std::vector<std::future<int>> fib_futures;
    for (int i = 20; i < 25; ++i) {
        auto future = pool.addTask(TaskPriority::CRITICAL, fibonacci, i);
        fib_futures.push_back(std::move(future));
    }

    // 等待一段时间
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 打印线程池状态
    pool.printStatus();

    // 等待所有任务完成
    pool.waitForAll();

    // 再次打印线程池状态
    pool.printStatus();

    // 获取斐波那契数列结果
    std::cout << "Fibonacci results:" << std::endl;
    for (size_t i = 0; i < fib_futures.size(); ++i) {
        int n = 20 + i;
        try {
            int result = fib_futures[i].get();
            std::cout << "fib(" << n << ") = " << result << std::endl;
        } catch (const std::exception& e) {
            std::cout << "fib(" << n << ") failed: " << e.what() << std::endl;
        }
    }

    // 停止线程池
    pool.stop();

    // 打印最终统计信息
    std::cout << "Final statistics:" << std::endl;
    auto stats = pool.getStatistics();
    std::cout << "Total tasks: " << stats.total_tasks << std::endl;
    std::cout << "Completed tasks: " << stats.completed_tasks << std::endl;
    std::cout << "Failed tasks: " << stats.failed_tasks << std::endl;
    std::cout << "Cancelled tasks: " << stats.cancelled_tasks << std::endl;
    std::cout << "Timeout tasks: " << stats.timeout_tasks << std::endl;
    std::cout << "Average execution time: " << stats.getAverageExecutionTime() << " ms" << std::endl;
    std::cout << "Success rate: " << stats.getSuccessRate() << "%" << std::endl;

    return 0;
}
