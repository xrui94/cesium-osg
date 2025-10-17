#pragma once

#include <CesiumAsync/ITaskProcessor.h>

#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

// 前向声明
namespace spdlog {
    class logger;
}


/**
 * A synchronous task processor that executes tasks immediately in the calling thread.
 * This avoids threading issues while maintaining compatibility with CesiumAsync::ITaskProcessor.
 */
class AsyncTaskProcessor : public CesiumAsync::ITaskProcessor
{
public:
    AsyncTaskProcessor();
    virtual ~AsyncTaskProcessor();

    virtual void startTask(std::function<void()> f) override;

    // Set whether to run tasks synchronously (true) or asynchronously (false)
    void setSynchronous(bool sync) { m_synchronous = sync; }
    bool isSynchronous() const { return m_synchronous; }

private:
	std::shared_ptr<spdlog::logger> m_logger;

    std::atomic<bool> m_synchronous{true}; // Default to synchronous execution
    
    // For async mode (if needed)
    std::atomic<bool> m_shutdown{false};
    std::thread m_workerThread;
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::queue<std::function<void()>> m_taskQueue;
    
    void workerThreadFunction();
    void startWorkerThread();
    void stopWorkerThread();
};
