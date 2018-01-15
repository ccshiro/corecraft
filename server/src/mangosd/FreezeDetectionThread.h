#ifndef MANGOSD__FREZE_DETECTION_THREAD_H
#define MANGOSD__FREZE_DETECTION_THREAD_H

#include "logging.h"
#include "World.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

class StuckError : public std::runtime_error
{
public:
    StuckError(const char* what) : std::runtime_error(what) {}
};

class FreezeDetectionThread
{
public:
    FreezeDetectionThread(unsigned int delay) : delay_(delay) {}

    void start()
    {
        thread_.reset(new std::thread(&FreezeDetectionThread::run, this));
    }

    void stop()
    {
        cond_.notify_all();
        thread_->join();
    }

    void run()
    {
        while (true)
        {
            auto previous = World::m_worldLoopCounter;

            std::unique_lock<std::mutex> lock(mutex_);
            if (cond_.wait_for(lock, std::chrono::seconds(delay_)) ==
                std::cv_status::no_timeout)
                return;

            if (World::m_worldLoopCounter == previous)
            {
                logging.error(
                    "World thread stopped updating. Freezer thread will now "
                    "kill the process.");
                printf("KILLING PROCESS!!!\n");
                fflush(stdout);
                throw StuckError(
                    "Killed process, update time exceeded MaxCoreStuckTime");
            }
        }
    }

private:
    unsigned int delay_;
    std::unique_ptr<std::thread> thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

#endif
