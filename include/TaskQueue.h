#include "Task.h"
#include<condition_variable>
#include<mutex>
#include<queue>
#include<unordered_map>
#include<unordered_set>
#include<vector>

namespace threadpool{

class TaskQueue{
public:
    TaskQueue() = default;
    ~TaskQueue() = default;

    // 禁止拷贝语义和移动语义
    TaskQueue(const TaskQueue&) = delete;
    TaskQueue(TaskQueue&&) = delete;
    TaskQueue& operator=(const TaskQueue&) = delete;
    TaskQueue& operator=(TaskQueue&&) = delete;

    // 添加任务
    void push(const std::shared_ptr<TaskBase>& task){
        std::lock_guard<std::mutex> lock(mutex_);
        tasks_.push(task);
        tasks_map_[task->getId()]=task;
        cv_.notify_one();
    }
    
    std::shared_ptr<TaskBase> pop(){
        std::unique_lock<std::mutex> lock(mutex_);
        // 直到队列不为空或者收到停止信号，该线程才被唤醒。
        cv_.wait(lock,[this](){
            return !tasks_.empty() || stop_;
        });
        if(stop_&&tasks_.empty()){
            return nullptr;
        }
        auto task = tasks_.top();
        tasks_.pop();
        return task;
    }
    // 带超时功能的
    template<class Rep,class Period>
    std::shared_ptr<TaskBase> tryPopFor(const std::chrono::duration<Rep,Period>& timeout){
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait_for(lock,timeout,[this](){
            return !tasks_.empty() || stop_;
        });
        if(stop_&& tasks_.empty()){
            return nullptr;
        }
        auto task = tasks_.top();
        tasks_.pop();
        return task;
    }
    size_t mySize() const{
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.size();
    }
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return tasks_.empty();
    }
    void clear(){
        std::lock_guard<std::mutex> lock(mutex_);
        while(!tasks_.empty()){
            tasks_.pop();
        }
        tasks_map_.clear();
        cv_.notify_all();
    }
    std::shared_ptr<TaskBase> getTask(uint64_t id) const{
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = tasks_map_.find(id);
        if(it!=tasks_map_.end()){
            return it->second;
        }
        return nullptr;
    }
    // 通知全部牛马
    void notifyAll(){
        cv_.notify_all();
    }
    void stop(){
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
        cv_.notify_all();
    }
    bool isStop() const{
        std::lock_guard<std::mutex> lock(mutex_);
        return stop_;
    }
private:
    mutable std::mutex mutex_;
    std::priority_queue<
        std::shared_ptr<TaskBase>,
        std::vector<std::shared_ptr<TaskBase>>,
        TaskComparator> tasks_;
    std::unordered_map<uint64_t,std::shared_ptr<TaskBase>> tasks_map_;
    std::condition_variable cv_;
    bool stop_ = false;
};

}