#include "../include/ThreadPoolCore.h"
#include <algorithm>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <iomanip>

namespace threadpool
{

    ThreadPoolCore::ThreadPoolCore(
        size_t min_threads,
        size_t max_threads,
        size_t idle_timeout_ms,
        bool enable_dynamic_scaling)
        : min_threads_(min_threads), max_threads_(max_threads), idle_timeout_ms_(idle_timeout_ms), enable_dynamic_scaling_(enable_dynamic_scaling), statue_(PoolStatus::INIT), task_queue_(std::make_unique<TaskQueue>()), next_task_id_(0), manager_(std::make_unique<ThreadPoolManager>())
    {
        if (min_threads_ > max_threads_)
        {
            std::swap(min_threads_, max_threads_);
        }
        if (max_threads_ == 0)
        {
            max_threads_ = 1;
            min_threads_ = 1;
        }
        workers_.reserve(max_threads_);
    }

    ThreadPoolCore::~ThreadPoolCore()
    {
        if (statue_ != PoolStatus::STOPPED)
        {
            shutdownNow();
        }
    }
    void ThreadPoolCore::start()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (statue_ != PoolStatus::INIT)
        {
            return;
        }
        statue_ = PoolStatus::RUNNING;
        for (size_t i = 0; i < min_threads_; i++)
        {
            workers_.emplace_back([this]()
                                  { worker(); });
        }
        manager_thread_ = std::thread([this]
                                      { manager(); });
    }
    void ThreadPoolCore::stop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (statue_ != PoolStatus::RUNNING)
        {
            return;
        }
        statue_ = PoolStatus::STOPPING;
        task_queue_->notifyAll();
        cv_.wait(lock, [this]
                 { return task_queue_->empty() && manager_->getActiveThreads() == 0; });

        lock.unlock();
        if (manager_thread_.joinable())
        {
            manager_thread_.join();
        }
        lock.lock();
        task_queue_->stop();
        for (auto &worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        workers_.clear();
        statue_ = PoolStatus::STOPPED;
    }
    void ThreadPoolCore::shutdownNow()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (statue_ == PoolStatus::STOPPED)
        {
            return;
        }
        statue_ = PoolStatus::STOPPING;
        task_queue_->clear();
        task_dependencies_.clear();
        task_queue_->notifyAll();
        lock.unlock();
        if (manager_thread_.joinable())
        {
            manager_thread_.join();
        }
        lock.lock();
        task_queue_->stop();
        for (auto &worker : workers_)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        workers_.clear();
        statue_ = PoolStatus::STOPPED;
    }
    void ThreadPoolCore::waitForAll()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]
                 { return statue_ != PoolStatus::RUNNING ||
                          (task_queue_->empty() && manager_->getActiveThreads() == 0); });
    }
    PoolStatus ThreadPoolCore::getStatus() const
    {
        return statue_.load();
    }
    size_t ThreadPoolCore::getThreadCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return workers_.size();
    }
    size_t ThreadPoolCore::getActivateThreadCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return manager_->getActiveThreads();
    }
    size_t ThreadPoolCore::getPendingTaskCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return task_queue_->mySize();
    }
    TaskStatistics ThreadPoolCore::getStatistics() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return manager_->getStatistics();
    }
    void ThreadPoolCore::resetStatistics()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        manager_->resetStatistics();
    }
    bool ThreadPoolCore::cancelTask(uint64_t id)
    {
        auto task = task_queue_->getTask(id);
        if (!task && task->getStatus() != TaskStatus::PENDING)
        {
            return false;
        }
        task->cancel();
        manager_->updateTaskStatistics(TaskStatus::CANCELLED);
        manager_->callPostTaskHook(id, TaskStatus::CANCELLED);
        return true;
    }
    TaskStatus ThreadPoolCore::getTaskStatus(uint64_t id) const
    {
        auto task = task_queue_->getTask(id);
        if (!task)
        {
            return TaskStatus::FAILED;
        }
        return task->getStatus();
    }
    std::chrono::milliseconds ThreadPoolCore::getTaskExecutionTime(uint64_t id) const
    {
        auto task = task_queue_->getTask(id);
        if (!task)
        {
            return std::chrono::milliseconds(0);
        }
        // TaskBase now exposes getExecutionTime(), so query it directly
        return task->getExecutionTime();
        return std::chrono::milliseconds(0);
    }
    bool ThreadPoolCore::addTaskDependency(uint64_t id, uint64_t depends_on_task)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto task = task_queue_->getTask(id);
        auto dep_task = task_queue_->getTask(depends_on_task);
        if (!task || !dep_task)
        {
            return false;
        }
        auto dep_list_it = task_dependencies_.find(id);
        if (dep_list_it != task_dependencies_.end())
        {
            if (dep_list_it->second.count(depends_on_task) > 0)
            {
                return true;
            }
        }
        task_dependencies_[id].insert(depends_on_task);
        return true;
    }
    void ThreadPoolCore::setPreTaskHook(std::function<void(uint64_t)> hook)
    {
        manager_->setPreTaskHook(std::move(hook));
    }
    void ThreadPoolCore::setPostTaskHook(std::function<void(uint64_t, TaskStatus)> hook)
    {
        manager_->setPostTaskHook(std::move(hook));
    }
    void ThreadPoolCore::setThreadLocalStorage(std::shared_ptr<void> storage)
    {
        auto thread_id = std::this_thread::get_id();
        std::lock_guard<std::mutex> lock(mutex_);
        thread_local_storage_[thread_id] = std::move(storage);
    }
    void ThreadPoolCore::setThreadLocalStorageInit(std::function<void()> init_func)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        thread_local_storage_init_ = std::move(init_func);
    }
    void ThreadPoolCore::printStatus() const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::cout << "===== ThreadPool Status =====" << std::endl;
        std::cout << "State: ";
        switch (statue_)
        {
        case PoolStatus::INIT:
            std::cout << "INIT";
            break;
        case PoolStatus::RUNNING:
            std::cout << "RUNNING";
            break;
        case PoolStatus::STOPPING:
            std::cout << "STOPPING";
            break;
        case PoolStatus::STOPPED:
            std::cout << "STOPPED";
            break;
        }
        std::cout << std::endl;

        std::cout << "Threads: " << workers_.size() << " (Active: " << manager_->getActiveThreads()
                  << ", Idle: " << manager_->getIdleThreads() << ")" << std::endl;
        std::cout << "Tasks: " << task_queue_->mySize() << " pending" << std::endl;

        auto stats = manager_->getStatistics();
        std::cout << "Statistics:" << std::endl;
        std::cout << "  Total tasks: " << stats.total_tasks << std::endl;
        std::cout << "  Completed tasks: " << stats.completed_tasks << std::endl;
        std::cout << "  Failed tasks: " << stats.failed_tasks << std::endl;
        std::cout << "  Cancelled tasks: " << stats.cancelled_tasks << std::endl;
        std::cout << "  Timeout tasks: " << stats.timeout_tasks << std::endl;
        std::cout << "  Average execution time: " << stats.getAverageExecutionTime() << " ms" << std::endl;
        std::cout << "  Success rate: " << std::fixed << std::setprecision(2)
                  << stats.getSuccessRate() << "%" << std::endl;
        std::cout << "============================" << std::endl;
    }
    void ThreadPoolCore::worker()
    {
        while (true)
        {
            std::shared_ptr<TaskBase> task;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                manager_->setIdleThreads(manager_->getIdleThreads() + 1);
                task = task_queue_->tryPopFor(std::chrono::milliseconds(100));
                manager_->setIdleThreads(manager_->getIdleThreads() - 1);
                // 没有任务并且任务队列接收到退出信号后才能退出。
                if (!task && task_queue_->isStop())
                {
                    return;
                }
                if (!task || task->getStatus() == TaskStatus::CANCELLED)
                {
                    continue;
                }
                // 未满足任务依赖则放回任务队列。
                if (!checkDependencies(task->getId()))
                {
                    task_queue_->push(task);
                    continue;
                }
                manager_->setActiveThreads(manager_->getActiveThreads() + 1);
            }
            manager_->callpreTaskHook(task->getId());
            task->execute();
            TaskStatus status = task->getStatus();
            manager_->callPostTaskHook(task->getId(), status);
            updateTaskStatistics(task->getId(), status);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                manager_->setActiveThreads(manager_->getActiveThreads() - 1);
            }
            cv_.notify_all();
        }
    }
    void ThreadPoolCore::manager()
    {
        while (statue_ == PoolStatus::RUNNING)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            if (statue_ != PoolStatus::RUNNING)
            {
                break;
            }
            if (enable_dynamic_scaling_)
            {
                adjustThreadPoolSize();
            }
        }
    }
    void ThreadPoolCore::adjustThreadPoolSize()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t current_threads = workers_.size();
        size_t pending_tasks = task_queue_->mySize();
        size_t active_threads = manager_->getActiveThreads();
        size_t idle_threads = manager_->getIdleThreads();

        if (pending_tasks > 0 && active_threads >= current_threads && current_threads < max_threads_)
        {
            size_t threads_to_add = std::min({pending_tasks, max_threads_ - current_threads, static_cast<size_t>(2)});
            for (size_t i = 0; i < threads_to_add; i++)
            {
                workers_.emplace_back([this]
                                      { worker(); });
            }
            // 如果有大量空闲线程，且当前线程数大于最小线程数，则减少线程
        }
        else if (idle_threads > current_threads / 2 && current_threads > min_threads_)
        {
            size_t threads_to_remove = std::min(
                {idle_threads / 2, current_threads - min_threads_, static_cast<size_t>(2)});

            // 我们不能直接删除线程，而是通过设置状态让空闲线程自然退出
            // 这里我们只是记录一下，实际退出逻辑在worker函数中
            // 由于C++线程无法直接终止，我们采用更复杂的方式：
            // 向任务队列中添加特殊任务，让线程接收到后退出
            // 但这种方式比较复杂，这里简化处理，不做实际减少
        }
    }
    bool ThreadPoolCore::checkDependencies(uint64_t task_id) const
    {
        auto dep_it = task_dependencies_.find(task_id);
        if (dep_it == task_dependencies_.end())
        {
            return true; // 没有依赖
        }

        // 检查所有依赖任务是否都已完成
        for (const auto& dep_task_id : dep_it->second)
        {
            auto task = task_queue_->getTask(dep_task_id);
            if (task)
            {
                auto status = task->getStatus();
                if (status != TaskStatus::COMPLETED)
                {
                    return false; // 依赖任务未完成
                }
            }
        }

        return true; // 所有依赖任务都已完成
    }

    void ThreadPoolCore::updateTaskStatistics(uint64_t task_id, TaskStatus status)
    {
        manager_->updateTaskStatistics(status);
    }
}