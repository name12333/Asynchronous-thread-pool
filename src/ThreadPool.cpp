#include "../include/ThreadPool.h"

ThreadPool::ThreadPool(int thread_count):stop_(false){
    for(int i =0;i<thread_count;i++){
        workers.emplace_back([this]{
            worker();
        });
    }
}

void ThreadPool::worker(){
    while(true){
        std::function<void()> task;
        {   
            // 管理互斥锁，这里是实现了加锁操作，该作用域内避免了竞态问题。
            std::unique_lock<std::mutex> lock(mutex_);
            // 条件等待：只有任务队列中有新任务需要执行，或者需要关闭线程池了，才往下执行，否则就将该线程进入休眠状态。
            cv_.wait(lock,[this]{
                return stop_ || !tasks.empty();
            });
            // 线程池停止，并且任务队列为空，则优雅的下班。
            if(stop_&&tasks.empty()) return;
            task = tasks.front();
            tasks.pop();
        }
        task(); // 将执行任务的过程放在被锁的区域外，使得该线程在执行该任务的时候，其他线程可以去访问任务队列，得到各自的工作。
    }
}
/* 版本一的实现。
void ThreadPool::addTask(std::function<void()> task){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        tasks.push(task);
    }
    // 通知牛马来活了，只需要通知一个牛马。
    cv_.notify_one();
}
*/
// ThreadPool析构函数，用于清理线程池资源
ThreadPool::~ThreadPool(){
    {
        // 使用lock_guard加锁，设置线程池停止标志
        // 作用域限定，确保在设置完stop_后立即释放锁
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    // 线程池状态改变了，需要所有牛马去查看“公告”。
    cv_.notify_all();
    // 然后将每个线程都阻塞执行完。
    for(auto& worker: workers){
        // joinable 用于检查线程对象是否关联了一个有效的执行线程。避免重复关闭线程。简单来说就是用来判断该线程是否已经启动且尚未被join或detach。
        if(worker.joinable()){
            worker.join();
        }
    }
}
