#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "cyclic/cyclic_runner.h"

#include <pthread.h>
#include <sched.h>

#include "utils/logger.h"

namespace mo_ecat
{

CyclicRunner::CyclicRunner(int cycle_time_us)
    : cycle_time_us_(cycle_time_us)
{
}

CyclicRunner::~CyclicRunner()
{
    Stop();
}

bool CyclicRunner::Start(Task task)
{
    if (running_) {
        LOG_WARN << "CyclicRunner already running";
        return false;
    }

    task_ = std::move(task);
    running_ = true;
    thread_ = std::thread(&CyclicRunner::RunLoop, this);
    ApplyThreadSettings();
    return true;
}

void CyclicRunner::Stop()
{
    if (!running_) {
        return;
    }

    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool CyclicRunner::IsRunning() const
{
    return running_;
}

void CyclicRunner::RunLoop()
{
    using namespace std::chrono;

    auto next_time = steady_clock::now();
    auto interval = microseconds(cycle_time_us_);

    while (running_) {
        if (task_) {
            task_();
        }

        // 基于绝对时间点睡眠，减少周期漂移。
        next_time += interval;
        std::this_thread::sleep_until(next_time);
    }
}

void CyclicRunner::SetRealtimePriority(int priority)
{
    realtime_priority_ = priority;
}

void CyclicRunner::SetCpuAffinity(int cpu_id)
{
    cpu_affinity_ = cpu_id;
}

void CyclicRunner::ApplyThreadSettings()
{
    if (realtime_priority_ >= 0) {
        sched_param param{};
        param.sched_priority = realtime_priority_;
        int ret = pthread_setschedparam(thread_.native_handle(), SCHED_FIFO, &param);
        if (ret != 0) {
            LOG_WARN << "CyclicRunner: failed to set realtime priority (errno=" << ret << ")";
        }
    }

    if (cpu_affinity_ >= 0) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_affinity_, &cpuset);
        int ret = pthread_setaffinity_np(thread_.native_handle(), sizeof(cpuset), &cpuset);
        if (ret != 0) {
            LOG_WARN << "CyclicRunner: failed to set CPU affinity (errno=" << ret << ")";
        }
    }
}

} // namespace mo_ecat
