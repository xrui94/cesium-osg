//#include "AsyncSystemWrapper.h"
//
//namespace czmosg
//{
//    OperationThreadTaskProcessor::OperationThreadTaskProcessor(uint32_t numThreads)
//    {
//        m_queue = new osg::OperationQueue;
//
//        for (uint32_t i = 0; i < numThreads; ++i)
//        {
//            auto thread = new osg::OperationThread;
//            thread->setOperationQueue(m_queue.get());
//            thread->startThread();
//            m_threads.push_back(thread);
//        }
//    }
//
//    OperationThreadTaskProcessor::~OperationThreadTaskProcessor()
//    {
//        stop();
//    }
//
//    void OperationThreadTaskProcessor::stop()
//    {
//        if (m_stopped) return;
//        m_stopped = true;
//
//        // 取消所有线程（会清空队列并停止循环）
//        for (auto& thread : m_threads)
//        {
//            thread->cancel(); // 这会导致线程退出 run() 循环
//        }
//
//        // 等待线程结束
//        for (auto& thread : m_threads)
//        {
//            thread->join();
//        }
//
//        m_threads.clear();
//        m_queue = nullptr;
//    }
//
//    void OperationThreadTaskProcessor::startTask(std::function<void()> f)
//    {
//        if (m_stopped) return;
//
//        // 包装函数为 osg::Operation 并加入队列
//        m_queue->add(new TaskOperation(std::move(f))); // false = add to back
//    }
//
//    // ====================================================================================
//
//    AsyncSystemWrapper::AsyncSystemWrapper()
//        : taskProcessor(std::make_shared<OperationThreadTaskProcessor>(4)),
//        asyncSystem(taskProcessor)
//    {
//    }
//
//    void AsyncSystemWrapper::shutdown()
//    {
//        asyncSystem.dispatchMainThreadTasks();
//    }
//
//    AsyncSystemWrapper& getAsyncSystemWrapper()
//    {
//        static AsyncSystemWrapper wrapper;
//        return wrapper;
//    }
//}
//
