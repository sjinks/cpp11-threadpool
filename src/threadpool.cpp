#include <mutex>
#include "threadpool.h"
#include "threadpool_p.h"
#include "threadpoolthread.h"
#include "runnable.h"

ThreadPool::ThreadPool()
	: d_ptr(new ThreadPoolPrivate())
{
}

ThreadPool::~ThreadPool()
{
	this->waitForDone();
}

void ThreadPool::start(Runnable* runnable, int priority)
{
	if (!runnable) {
		return;
	}

	auto* d = this->d_func();
	std::unique_lock<std::mutex> locker(d->mutex);
	if (!d->tryStart(runnable)) {
		d->enqueueTask(runnable, priority);

		if (!d->waitingThreads.empty()) {
			auto* t = d->waitingThreads.front();
			d->waitingThreads.pop_front();
			t->runnableReady.notify_one();
		}
	}
}

bool ThreadPool::tryStart(Runnable* runnable)
{
	if (!runnable) {
		return false;
	}

	auto* d = this->d_func();
	std::unique_lock<std::mutex> locker(d->mutex);

	if (d->allThreads.empty() && d->activeThreadCount() >= d->maxThreadCount) {
		return false;
	}

	return d->tryStart(runnable);
}

unsigned long int ThreadPool::expiryTimeout() const
{
	return this->d_func()->expiryTimeout;
}

void ThreadPool::setExpiryTimeout(unsigned long int v)
{
	this->d_func()->expiryTimeout = v;
}

std::size_t ThreadPool::maxThreadCount() const
{
	return this->d_func()->maxThreadCount;
}

void ThreadPool::setMaxThreadCount(std::size_t n)
{
	auto* d = this->d_func();
	if (d->maxThreadCount == n) {
		return;
	}

	d->maxThreadCount = n;
	d->tryToStartMoreThreads();
}

std::size_t ThreadPool::activeThreadCount() const
{
	auto* d = this->d_func();
	const std::unique_lock<std::mutex> locker(d->mutex);
	return d->activeThreadCount();
}

std::size_t ThreadPool::queueSize() const
{
	return this->d_func()->queue.size();
}

void ThreadPool::reserveThread()
{
	auto* d = this->d_func();
	const std::unique_lock<std::mutex> locker(d->mutex);
	++d->reservedThreads;
}

void ThreadPool::releaseThread()
{
	auto* d = this->d_func();
	const std::unique_lock<std::mutex> locker(d->mutex);
	--d->reservedThreads;
	d->tryToStartMoreThreads();
}

bool ThreadPool::waitForDone(unsigned long int msec)
{
	auto* d = this->d_func();
	auto ret = d->waitForDone(msec);
	if (ret) {
		d->reset();
	}

	return ret;
}

void ThreadPool::clear()
{
	this->d_func()->clear();
}

void ThreadPool::cancel(Runnable* runnable)
{
	auto* d = this->d_func();
	if (!d->stealRunnable(runnable)) {
		return;
	}

	if (runnable->autoDelete() && !--runnable->m_ref) {
		delete runnable;
	}
}
