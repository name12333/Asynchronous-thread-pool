/* 引入状态机（Init,Running，Stopping，Stopped）概念*/
#pragma once
#include<vector>
#include<thread>
#include<condition_variable>
#include<mutex>
#include<atomic>
#include<future>
#include<functional>
#include<memory>
#include<queue>
#include<stdexcept>
#include<type_traits> 

class ThreadPool{
public:
    // 状态要被外界可视
    enum class PoolState{
        Init,
        Running,
        Stopping,
        Stopped
    };
    explicit ThreadPool(size_t thread_min = 4,size_t thread_max = std::thread::hardware_concurrency());
    ~ThreadPool();
    // 禁止复制语义
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    // 允许移动语义
    ThreadPool(ThreadPool&& other) noexcept;
    ThreadPool& operator=(ThreadPool&& other) noexcept;
    
    void start();
    void stop(); //优雅关闭：即清空任务队列了再结束。
    void shutdown_now(); // 强制关闭

    void waitForAll();
    
    // 带优先级的任务
    template<class Func,class... Args>
    auto addTask(int priority,Func&& func,Args&&... args)->std::future<std::invoke_result_t<Func,Args...>>{
        using return_type = std::invoke_result_t<Func,Args...>;
        auto bound_task = std::bind(std::forward<Func>(func),std::forward<Args>(args)...);
        auto task =std::make_shared<std::packaged_task<return_type()>>(bound_task);
        std::future<return_type> res = task->get_furture();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(state_!=PoolState::Running){
                throw std::runtime_error("ThreadPool is not running");
            }
            tasks.push(TaskItem{
                priority,
                [task]{
                    (*task)();
                }
            });
        }
        cv_.notify_one();
        return res;
    }
    // 无优先级的任务
    template<class Func,class... Args>
    auto addTask(Func&& func,Args&&... args)->std::future<std::invoke_result_t<Func,Args...>>{
        //使用委派思想，减少开销。
        return addTask(0,std::forward<Func>(func),std::forward<Args>(args)...);
    }
private:
    void worker();
    struct TaskItem{
        int priority;
        std::function<void()> task;
        bool operator<(const TaskItem& other) const{
            return priority < other.priority;
        }
    };
private:
    size_t thread_min_,thread_max_;
    std::vector<std::thread> workers;
    std::priority_queue<TaskItem> tasks;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<PoolState> state_;
    std::atomic<size_t> active_task_count; // 用于线程安全地追踪当前正在执行的任务数量。
};

