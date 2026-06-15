#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace mo_ecat
{

// 周期任务调度器：在独立线程中按固定周期反复执行用户任务。
class CyclicRunner
{
public:
    using Task = std::function<void()>;

    explicit CyclicRunner(int cycle_time_us);
    ~CyclicRunner(); // 析构时自动停止线程

    // 启动周期线程，重复调用返回 false。
    bool Start(Task task);

    // 停止并等待线程退出，可重复调用。
    void Stop();

    bool IsRunning() const;

private:
    void RunLoop();

    int cycle_time_us_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    Task task_;
};

} // namespace mo_ecat
