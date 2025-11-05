#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <future>
#include <functional>
#include <condition_variable>
#include <atomic>

class ThreadPool
{
public:
    ThreadPool(size_t n) : stop_flag(false)
    {
        for (size_t i = 0; i < n; ++i)
        {
            workers.emplace_back([this]
                                 {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->cond.wait(lock, [this]{ return this->stop_flag || !this->tasks.empty(); });
                        if (this->stop_flag && this->tasks.empty()) return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }
                    task();
                } });
        }
    }
    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop_flag = true;
        }
        cond.notify_all();
        for (auto &t : workers)
            if (t.joinable())
                t.join();
    }

    template <typename F, typename... Args>
    auto enqueue(F &&f, Args &&...args) -> std::future<decltype(f(args...))>
    {
        using return_t = decltype(f(args...));
        auto task_ptr = std::make_shared<std::packaged_task<return_t()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<return_t> res = task_ptr->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace([task_ptr]()
                          { (*task_ptr)(); });
        }
        cond.notify_one();
        return res;
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cond;
    bool stop_flag;
};
