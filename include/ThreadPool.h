#pragma once
#include<thread>
#include<mutex>
#include<condition_variable>
#include<queue>
#include<functional>
#include<vector>
#include<atomic>
#include<future>

class ThreadPool{
public:
    explicit ThreadPool(int thread_count=4);
    ~ThreadPool();
    // void addTask(std::function<void()> task);
    /** 
     * 针对版本一的addTask只能执行任务，不能拿到返回值的问题进行了改善。
     * 没有返回值会导致的问题： 1. 无法知道任务什么时候完成 2.无法获取计算结果 3. 无法做到依赖调度。
     * 因此，版本二中，addTask需要支持任意函数、任意参数、任意返回类型。
    */
   // 模板在头文件中定义了，也应该在头文件中实现。
   template<class Func,class... Args>
    auto addTask(Func&& func,Args&&... args)->std::future<std::invoke_result_t<Func,Args...>>{
        using return_type = std::invoke_result_t<Func,Args...>;
    // 由于我们的packaged_task 只接受无参函数，但实际的函数不一定是无参的，因此，我们使用bind函数，将args绑定到func上，相当于提前将参数传递给函数。
        auto bound_task = std::bind(std::forward<Func>(func),std::forward<Args>(args)...);
        // 将无参函数 bound_task转化为异步任务函数。
        auto task_ptr = std::make_shared<std::packaged_task<return_type()>>(std::move(bound_task));
        //作用：std::future 和 std::promise/std::packaged_task 配合使用，允许你在未来某个时刻通过 res.get() 获取异步任务的返回值，调用时会阻塞直到结果可用。
        std::future<return_type> res = task_ptr->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks.emplace([task_ptr]{
                (*task_ptr)();
            });
        }
        // 通知一个牛马干活。
        cv_.notify_one();
        return res;
    }
private:
    void worker();
private:
    std::vector<std::thread> workers; //工作线程
    std::queue<std::function<void()>> tasks; // 任务队列
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_; // 线程池状态（是否结束）
};