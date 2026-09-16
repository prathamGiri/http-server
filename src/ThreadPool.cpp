#include "ThreadPool.hpp"

ThreadPool::ThreadPool(int numThreads){
    for (int i = 0; i < numThreads; i++)
    {
        workers.emplace_back(&ThreadPool::workerLoop, this);
    }
}

ThreadPool::~ThreadPool() {
    stop = true;
    cv.notify_all();
    for(auto& worker : workers){
        if (worker.joinable()) worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        tasks.push(std::move(task));
    }
    cv.notify_one();
}

void ThreadPool::workerLoop() {
    while(true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, [this]{ return stop || !tasks.empty(); });

            if (stop && tasks.empty()) return;

            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
    }
}