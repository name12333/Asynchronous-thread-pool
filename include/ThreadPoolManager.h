#pragma once
#include "ThreadPoolTypes.h"
#include<functional>
#include<mutex>
#include<atomic>
#include<iostream>

namespace threadpool{

class ThreadPoolManager{
public:
    using PreTaskHook = std::function<void(uint64_t)>;
    using PostTaskHook = std::function<void(uint64_t,TaskStatus)>;
    ThreadPoolManager() = default;
    ~ThreadPoolManager() = default;
    //禁止拷贝语义和移动语义
    ThreadPoolManager(const ThreadPoolManager&) = delete;
    ThreadPoolManager(ThreadPoolManager&&) = delete;
    ThreadPoolManager& operator=(const ThreadPoolManager&) = delete;
    ThreadPoolManager& operator=(ThreadPoolManager&&) = delete;
    // 设置任务前钩子,这里不用引用传递的原因：这种写法允许在传入临时对象（右值）时完全避免深拷贝，而在传入持久对象（左值）时保持与引用传递相当的性能（仅多一次廉价的移动操作）。
    void setPreTaskHook(PreTaskHook hook){
        std::lock_guard<std::mutex> lock(mutex_);
        pre_task_hook_ = std::move(hook);
    }
    void setPostTaskHook(PostTaskHook hook){
        std::lock_guard<std::mutex> lock(mutex_);
        post_task_hook_ = std::move(hook);
    }
    // 调用钩子
    void callpreTaskHook(uint64_t id){
        std::lock_guard<std::mutex> lock(mutex_);
        /*
        std::function 重载了 operator bool。
        当它内部存储了一个有效的可调用对象（函数、lambda 等）时，返回 true；
        如果为空（默认构造），返回 false。
        */
        if(pre_task_hook_){
            pre_task_hook_(id);
        }
    }
    void callPostTaskHook(uint64_t id,TaskStatus status){
        std::lock_guard<std::mutex> lock(mutex_);
        if(post_task_hook_){
            post_task_hook_(id,status);
        }
    }
    // 更新统计信息
    void updateTaskStatistics(TaskStatus status){
        std::lock_guard<std::mutex> lock(mutex_);
        switch(status){
            case TaskStatus::COMPLETED:
                task_statistics_.completed_tasks++;
                break;
            case TaskStatus::CANCELLED:
                task_statistics_.cancelled_tasks++;
                break;
            case TaskStatus::FAILED:
                task_statistics_.failed_tasks++;
                break;
            case TaskStatus::TIMEOUT:
                task_statistics_.timeout_tasks++;
                break;
            default:
                std::cerr<<"No such task status"<<std::endl;
        }
    }
    // 增加总任务数量
    void IncreaseTotalNumber(){
        std::lock_guard<std::mutex> lock(mutex_);
        task_statistics_.total_tasks++;
    }
    TaskStatistics getStatistics() const{
        std::lock_guard<std::mutex> lock(mutex_);
        return task_statistics_;
    }
    void resetStatistics(){
        std::lock_guard<std::mutex> lock(mutex_);
        task_statistics_ = TaskStatistics();
    }
    void addExecutionTime(std::chrono::milliseconds time){
        std::lock_guard<std::mutex> lock(mutex_);
        task_statistics_.total_execution_time+=time;
    }
    size_t getActiveThreads() const{
        return activate_threads_.load();
    }
    void setActiveThreads(size_t count){
        activate_threads_ = count;
    }
    void setIdleThreads(size_t count){
        idle_threads_ = count;
    }
    size_t getIdleThreads() const{
        return idle_threads_.load();
    }

private:
    mutable std::mutex mutex_;
    PreTaskHook pre_task_hook_;
    PostTaskHook post_task_hook_;
    TaskStatistics task_statistics_;
    std::atomic<size_t> activate_threads_{0};
    std::atomic<size_t> idle_threads_{0};
};

}