cmake_minimum_required(VERSION 3.10)
project(AsynchronousThreadPool)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)

# 收集源文件
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 创建线程池库
add_library(threadpool STATIC ${SOURCES})
target_include_directories(threadpool PUBLIC include)

# 链接线程库
find_package(Threads REQUIRED)
target_link_libraries(threadpool PRIVATE Threads::Threads)
