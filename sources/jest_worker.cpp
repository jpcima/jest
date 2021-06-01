#include "jest_worker.h"
#include <thread>
#include <mutex>
#include <condition_variable>

namespace jest {

struct Worker::Impl {
    Worker *_self = nullptr;
    volatile bool _quit = false;
    std::unique_ptr<CompileRequest> _req;
    std::thread _thread;
    std::mutex _mutex;
    std::condition_variable _cond;

    void performWork();
};

///
Worker::Worker(QObject *parent)
    : QObject(parent),
      _impl(new Impl)
{
    Impl &impl = *_impl;

    impl._self = this;
    impl._thread = std::thread([&impl]() { impl.performWork(); });

    connect(
        this, &Worker::startedCompilingPrivate,
        this, &Worker::startedCompiling, Qt::QueuedConnection);
    connect(
        this, &Worker::finishedCompilingPrivate,
        this, &Worker::finishedCompiling, Qt::QueuedConnection);
}

Worker::~Worker()
{
    Impl &impl = *_impl;

    std::unique_lock<std::mutex> lock(impl._mutex);
    impl._quit = true;
    impl._cond.notify_one();
    lock.unlock();
    impl._thread.join();
}

void Worker::request(const CompileRequest &request)
{
    Impl &impl = *_impl;

    std::unique_lock<std::mutex> lock(impl._mutex);
    impl._req.reset(new CompileRequest(request));
    impl._cond.notify_one();
    lock.unlock();
}

///
void Worker::Impl::performWork()
{
    for (;;) {
        std::unique_lock<std::mutex> lock(_mutex);

        if (_quit)
            break;

        if (!_req)
            _cond.wait(lock);

        if (_quit)
            break;

        std::unique_ptr<CompileRequest> req = std::move(_req);
        emit _self->startedCompilingPrivate(*req);
        CompileResult result = DSPWrapper::compile(*req);
        emit _self->finishedCompilingPrivate(*req, result);
    }
}

} // namespace jest
