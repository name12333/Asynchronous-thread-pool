#include "../include/ThreadPool.h"
#include<iostream>

void func(int id){
    std::cout<<"线程："<<std::this_thread::get_id()<<"执行任务："<<id<<std::endl;
}
void test_v1(ThreadPool& pool){
    for(int i =0;i<10;i++){
        pool.addTask([i]{
            func(i);
        });
    }
}
void test_v2(ThreadPool& pool){
    std::vector<std::future<void>> results;
    for(int i =0;i<10;i++){
        results.push_back(pool.addTask([i]{
            func(i);
        }));
    }
    for(auto& f:results){
        f.get();
    }
}
int main(){
    ThreadPool pool(4);
    test_v2(pool);
    return 0;
}