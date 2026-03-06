#pragma once
#include<chrono>

namespace threadpool{

// 任务优先级枚举
enum class TaskPriority{
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// 任务状态枚举
enum class TaskStatus{
    PENDING, // 等待执行
    RUNNING, // 正在执行
    COMPLETED, // 已完成
    CANCELLED, // 已取消
    TIMEOUT, // 超时
    FAILED // 执行失败
};
// 线程池状态枚举
enum class PoolStatus{
    INIT,
    RUNNING,
    STOPPING,
    STOPPED
};
// 任务统计信息结构
struct TaskStatistics{
    size_t total_tasks = 0;
    size_t completed_tasks = 0;
    size_t failed_tasks = 0;
    size_t timeout_tasks = 0;
    size_t cancelled_tasks = 0;
    std::chrono::milliseconds total_execution_time{0};

    double getAverageExecutionTime() const{
        if(completed_tasks == 0) return 0.0;
        return static_cast<double>(total_execution_time.count())/completed_tasks;
    }
    double getSuccessRate() const{
        if(total_tasks==0) return 0.0;
        return static_cast<double>(completed_tasks) / total_tasks * 100;
    }
};

}