#include "../include/ThreadPool_v3.h"

ThreadPool::ThreadPool(size_t thread_min,size_t thread_max):thread_min_(thread_min),thread_max_(thread_max),state_(PoolState::Init),active_task_count(0){
    workers.reserve(thread_min);
}
ThreadPool::~ThreadPool(){
    stop();
}
// 不允许在Running状态下移动，防止线程还在执行或worker还在使用this指针，从而导致程序崩溃
ThreadPool::ThreadPool(ThreadPool&& other) noexcept{
        // 由于标记为noexcept，这里不能重新抛出异常
        // 实际应用中可能需要根据需求调整
    // 使用互斥锁保护线程池状态
    std::lock_guard<std::mutex> lock(mutex_);
    // 检查源线程池是否正在运行，如果是则抛出异常
    if(other.state_==PoolState::Running){
        throw std::runtime_error("Cannot move a running ThreadPool!");
    }
    // 转移工作线程队列
    workers = std::move(other.workers);
    // 转移任务队列
    tasks = std::move(other.tasks);
    // 转移最小线程数
    thread_min_ = std::move(other.thread_min_);
    // 转移最大线程数
    thread_max_ = std::move(other.thread_max_);
    // 原子类型的拷贝赋值运算符被显式删除了，因此不能直接将一个原子对象赋值给另一个原子对象，那么如何解决这个问题呢？我们呢可以使用原子对象的load（）读取值，使用其store()写入值。
    // state_ = std::move(other.state_);
    state_.store(other.state_.load());
    active_task_count.store(other.active_task_count.load());
    // 将被转移的对象进行逻辑删除。
    other.state_ = PoolState::Stopped;
    other.active_task_count.store(0);
    other.thread_min_ = 0;
    other.thread_max_ = 0;
}
