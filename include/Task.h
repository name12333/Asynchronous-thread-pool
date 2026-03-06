#pragma once
#include "ThreadPoolTypes.h"
#include<functional>
#include<atomic>
#include<mutex>
#include<memory>
#include<future>
#include<chrono>

namespace threadpool{

class TaskBase{
public:
    virtual ~TaskBase() = default;
    virtual void execute() = 0;
    virtual void cancel() = 0;
    virtual TaskStatus getStatus() const = 0;
    virtual uint64_t getId() const = 0;
    virtual TaskPriority getPriority() const = 0;
    virtual std::chrono::milliseconds getExecutionTime() const {
        return std::chrono::milliseconds(0);
    }
};
// 具体任务类
template<class F,class... Args>
class Task:public TaskBase{
public:
    using ReturnType = typename std::invoke_result<F, Args...>::type;

    Task(uint64_t id,TaskPriority priority,F&& func,Args&&... args)
        :id_(id), priority_(priority),status_(TaskStatus::PENDING)
    {
        // 绑定任务
        task_ = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(func),std::forward<Args>(args)...)
        );
    }
    void execute() override{
        std::unique_lock<std::mutex> lock(mutex_);
        if(status_ != TaskStatus::PENDING) return;
        status_ = TaskStatus::RUNNING;
        lock.unlock();
        
        auto start_time = std::chrono::steady_clock::now();

        try{
            (*task_)();
            lock.lock();
            status_ = TaskStatus::COMPLETED;
        }catch(const std::exception& e){
            lock.lock();
            status_ = TaskStatus::FAILED;
            error_message_ = e.what();
        }catch(...){
            lock.lock();
            status_ = TaskStatus::FAILED;
            error_message_ = "Unknown error";
        }
        auto end_time = std::chrono::steady_clock::now();
        execution_time_ = std::chrono::duration_cast<std::chrono::milliseconds>(end_time-start_time);
    }
    void cancel() override{
        std::lock_guard<std::mutex> lock(mutex_);
        if(status_ == TaskStatus::PENDING){
            status_ = TaskStatus::CANCELLED;
        }
    }
    TaskStatus getStatus() const override{
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }
    uint64_t getId() const override{
        std::lock_guard<std::mutex> lock(mutex_);
        return id_;
    }
    TaskPriority getPriority() const override{
        std::lock_guard<std::mutex> lock(mutex_);
        return priority_;
    }

    std::chrono::milliseconds getExecutionTime() const{
        std::lock_guard<std::mutex> lock(mutex_);
        return execution_time_;
    }
    std::string getErrorMessage() const{
        std::lock_guard<std::mutex> lock(mutex_);
        return error_message_;
    }
    std::future<ReturnType> getFuture(){
        return task_->get_future();
    }

private:
    uint64_t id_;
    TaskPriority priority_;
    std::shared_ptr<std::packaged_task<ReturnType()>> task_;
    std::atomic<TaskStatus> status_;
    std::chrono::milliseconds execution_time_{0};
    std::string error_message_;
    mutable std::mutex mutex_;
};

// 任务比较器，用于优先队列
struct TaskComparator{
    bool operator()(const std::shared_ptr<TaskBase>& a,const std::shared_ptr<TaskBase>& b){
        return static_cast<int>(a->getPriority())< static_cast<int>(b->getPriority());
    }
};

}