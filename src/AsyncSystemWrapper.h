//#pragma once
//
//#include <osg/OperationThread>
//
//#include <CesiumAsync/ITaskProcessor.h>
//#include <CesiumAsync/AsyncSystem.h>
//
//#include <functional>
//
//namespace czmosg
//{
//
//    class TaskOperation : public osg::Operation
//    {
//    public:
//        explicit TaskOperation(std::function<void()> func)
//            : osg::Operation("TaskOperation", false)  // false = not a database operation
//            , _func(std::move(func))
//        {
//        }
//
//        void operator()(osg::Object* caller) override
//        {
//            if (_func)
//                _func();
//        }
//
//    private:
//        std::function<void()> _func;
//    };
//
//    // Implement Cesium's ITaskprocessor interface, the basis for its AsyncSystem.
//    class OperationThreadTaskProcessor: public CesiumAsync::ITaskProcessor
//    {
//    public:
//        OperationThreadTaskProcessor(uint32_t numThreads);
//        ~OperationThreadTaskProcessor();
//        virtual void startTask(std::function<void()> f) override;
//        virtual void stop();
//    private:
//        //osg::ref_ptr<osg::OperationThread> m_operationThreads;
//
//        osg::ref_ptr<osg::OperationQueue> m_queue;
//        std::vector<osg::ref_ptr<osg::OperationThread>> m_threads;
//        bool m_stopped = false;
//    };
//
//    // Wrapper class that allows shutting down the AsyncSystem when the program exits.
//    class AsyncSystemWrapper
//    {
//    public:
//        AsyncSystemWrapper();
//        CesiumAsync::AsyncSystem& getAsyncSystem() noexcept;
//        void shutdown();
//        std::shared_ptr<OperationThreadTaskProcessor> taskProcessor;
//        CesiumAsync::AsyncSystem asyncSystem;
//    };
//
//    AsyncSystemWrapper& getAsyncSystemWrapper();
//    inline CesiumAsync::AsyncSystem& getAsyncSystem() noexcept
//    {
//        return getAsyncSystemWrapper().asyncSystem;
//    }
//}
