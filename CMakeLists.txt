# 指定 CMake 最低版本
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(ThreadPoolProject)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/include)

# 1. 定义源文件变量
# 收集 src 目录下的所有源文件
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
)

# 2. 创建静态库 (libThreadPool.a 或 libThreadPool.lib)
add_library(ThreadPool STATIC ${SOURCES})

# 3. 设置测试可执行文件
# 指定测试源文件
add_executable(test_exe test/test.cpp)

# 4. 链接库
# 将 ThreadPool 库链接到测试程序中
target_link_libraries(test_exe ThreadPool)

# 如果是 Linux 系统，通常需要链接 pthread 库
# find_package(Threads REQUIRED)
# target_link_libraries(test_exe ThreadPool Threads::Threads)
