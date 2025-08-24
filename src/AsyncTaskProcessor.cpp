#include "AsyncTaskProcessor.h"

#include <iostream>
#include <exception>


AsyncTaskProcessor::AsyncTaskProcessor()
{
	// Default to synchronous execution to avoid threading issues
	// Worker thread will only be started if setSynchronous(false) is called
}

AsyncTaskProcessor::~AsyncTaskProcessor()
{
	stopWorkerThread();
}

void AsyncTaskProcessor::startTask(std::function<void()> f)
{
	if (m_synchronous) {
		// 同步执行
		try {
			f();
		}
		catch (const std::exception& e) {
			std::cerr << "Synchronous task execution failed: " << e.what() << std::endl;
		}
		catch (...) {
			std::cerr << "Synchronous task execution failed with unknown exception" << std::endl;
		}
	}
	else {
		// 异步执行
		{
			std::lock_guard<std::mutex> lock(m_queueMutex);
			m_taskQueue.push(f);
		}

		// 确保工作线程已启动
		if (!m_workerThread.joinable() && !m_shutdown) {
			startWorkerThread();
		}

		m_condition.notify_one();
	}
}

void AsyncTaskProcessor::startWorkerThread()
{
	if (!m_workerThread.joinable() && !m_shutdown) {
		m_workerThread = std::thread(&AsyncTaskProcessor::workerThreadFunction, this);
	}
}

void AsyncTaskProcessor::stopWorkerThread()
{
	m_shutdown = true;
	m_condition.notify_all();

	if (m_workerThread.joinable()) {
		m_workerThread.join();
	}
}

void AsyncTaskProcessor::workerThreadFunction()
{
	while (!m_shutdown) {
		std::function<void()> task;

		{
			std::unique_lock<std::mutex> lock(m_queueMutex);
			m_condition.wait(lock, [this] { return m_shutdown || !m_taskQueue.empty(); });

			if (m_shutdown && m_taskQueue.empty()) {
				break;
			}

			if (!m_taskQueue.empty()) {
				task = m_taskQueue.front();
				m_taskQueue.pop();
			}
		}

		if (task) {
			try {
				task();
			}
			catch (const std::exception& e) {
				std::cerr << "Asynchronous task execution failed: " << e.what() << std::endl;
			}
			catch (...) {
				std::cerr << "Asynchronous task execution failed with unknown exception" << std::endl;
			}
		}
	}
}
