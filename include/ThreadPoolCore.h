#pragma once
#include "TaskQueue.h"
#include "Task.h"
#include "ThreadPoolManager.h"
#include<thread>

namespace threadpool{

class ThreadPoolCore{
public:
    explicit ThreadPoolCore(
        size_t min_threads=std::thread::hardware_concurrency()/2,
        size_t max_threads=std::thread::hardware_concurrency(),
        size_t idle_time_out_ms = 60000,
        bool enable_dynamic_scaling = true
    );
    ~ThreadPoolCore();
    //禁止拷贝语义，允许移动语义
    ThreadPoolCore(const ThreadPoolCore&) = delete;
    ThreadPoolCore(ThreadPoolCore&&) noexcept = default;
    ThreadPoolCore& operator=(const ThreadPoolCore&) = delete;
    ThreadPoolCore& operator=(ThreadPoolCore&&) noexcept = default;
    
    void start();
    void stop();
    void shutdownNow();
    void waitForAll(); //等待所有任务完成

    // 获取线程池状态，当前线程数，活跃线程数，等待中的任务数，任务统计信息
    PoolStatus getStatus() const;
    size_t getThreadCount() const;
    size_t getActivateThreadCount() const;
    size_t getPendingTaskCount() const;
    TaskStatistics getStatistics() const;

    // 重置统计信息、添加任务（有/无 优先级）
    void resetStatistics();
    // 有优先级
    template<class F,class... Args>
    auto addTask(TaskPriority priority,F&& func,Args&&... args)->std::future<std::invoke_result_t<F,Args...>>{
        using ReturnType = std::invoke_result_t<F,Args...>;
        
        if(statue_ != PoolStatus::RUNNING){
            throw std::runtime_error("This thread pool is not running!");
        }
        uint64_t id = next_task_id_++;
        auto task = std::make_shared<Task<F, Args...>>(
            id,priority,std::forward<F>(func),std::forward<Args>(args)...
        );
        auto future = task->getFuture();
        task_queue_->push(task);
        manager_->IncreaseTotalNumber();
        if(enable_dynamic_scaling_){
            adjustThreadPoolSize();
        }
        return future;
    }
    template<class F,class... Args>
    auto addTask(F&& func,Args&&... args)->std::future<std::invoke_result_t<F,Args...>>{
        return addTask(TaskPriority::NORMAL,std::forward<F>(func),std::forward<Args>(args)...);
    }
    bool cancelTask(uint64_t id);
    TaskStatus getTaskStatus(uint64_t id) const;
    std::chrono::milliseconds getTaskExecutionTime(uint64_t id) const;

    bool addTaskDependency(uint64_t task_1,uint64_t task_2);

    void setPreTaskHook(std::function<void(uint64_t)> hook);
    void setPostTaskHook(std::function<void(uint64_t,TaskStatus)> hook);

    void setThreadLocalStorageInit(std::function<void()> init_func);

    template<class T>
    std::shared_ptr<T> getThreadLocalStorage(){
        auto thread_id = std::this_thread::get_id();
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = thread_local_storage_.find(thread_id);
        if(it!= thread_local_storage_.end()){
            return std::static_pointer_cast<T>(it->second);
        }

        if(thread_local_storage_init_){
            thread_local_storage_init_();
            it = thread_local_storage_.find(thread_id);
            if(it!=thread_local_storage_.end()){
                return std::static_pointer_cast<T>(it->second);
            }
        }
        return nullptr;
    }
    void setThreadLocalStorage(std::shared_ptr<void> storage);
    void printStatus() const;

private:
    void adjustThreadPoolSize();
    void worker();
    void manager();
    bool checkDependencies(uint64_t id) const;
    void updateTaskStatistics(uint64_t id,TaskStatus status);
private:
    // 线程池参数
    size_t min_threads_, max_threads_;
    size_t idle_timeout_ms_;
    bool enable_dynamic_scaling_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;

    std::atomic<uint64_t> next_task_id_;
    std::atomic<PoolStatus> statue_;
    std::vector<std::thread> workers_;
    std::thread manager_thread_;
    std::unique_ptr<TaskQueue> task_queue_;
    std::unordered_map<uint64_t,std::unordered_set<uint64_t>> task_dependencies_;
    std::unique_ptr<ThreadPoolManager> manager_;

    std::function<void(uint64_t)> pre_task_hook_;
    std::function<void(uint64_t,TaskStatus)> post_task_hook_;

    std::function<void()> thread_local_storage_init_;
    std::unordered_map<std::thread::id,std::shared_ptr<void>> thread_local_storage_;
    
};

}