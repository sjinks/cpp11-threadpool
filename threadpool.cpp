#include <mutex>
#include "threadpool.h"
#include "threadpool_p.h"
#include "threadpoolthread.h"
#include "runnable.h"

ThreadPool::ThreadPool(void)
	: d_ptr(new ThreadPoolPrivate())
{
}

ThreadPool::~ThreadPool(void) noexcept
{
	this->waitForDone();
}

void ThreadPool::start(Runnable* runnable, int priority)
{
	if (!runnable) {
		return;
	}

	ThreadPoolPrivate* const d = this->d_func();
	std::unique_lock<std::mutex> locker(d->mutex);
	if (!d->tryStart(runnable)) {
		d->enqueueTask(runnable, priority);

		if (!d->waitingThreads.empty()) {
			ThreadPoolThread* t = d->waitingThreads.front();
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

	ThreadPoolPrivate* const d = this->d_func();
	std::unique_lock<std::mutex> locker(d->mutex);

	if (d->allThreads.empty() && d->activeThreadCount() >= d->maxThreadCount) {
		return false;
	}

	return d->tryStart(runnable);
}

unsigned long int ThreadPool::expiryTimeout(void) const
{
	return this->d_func()->expiryTimeout;
}

void ThreadPool::setExpiryTimeout(unsigned long int v)
{
	this->d_func()->expiryTimeout = v;
}

std::size_t ThreadPool::maxThreadCount(void) const
{
	return this->d_func()->maxThreadCount;
}

void ThreadPool::setMaxThreadCount(std::size_t n)
{
	ThreadPoolPrivate* const d = this->d_func();
	if (d->maxThreadCount == n) {
		return;
	}

	d->maxThreadCount = n;
	d->tryToStartMoreThreads();
}

std::size_t ThreadPool::activeThreadCount(void) const
{
	const ThreadPoolPrivate* const d = this->d_func();
	std::unique_lock<std::mutex> locker(d->mutex);
	return d->activeThreadCount();
}

std::size_t ThreadPool::queueSize(void) const
{
	return this->d_func()->queue.size();
}

void ThreadPool::reserveThread(void)
{
	ThreadPoolPrivate* const d = this->d_func();
	std::unique_lock<std::mutex> locker(d->mutex);
	++d->reservedThreads;
}

void ThreadPool::releaseThread(void)
{
	ThreadPoolPrivate* const d = this->d_func();
	std::unique_lock<std::mutex> locker(d->mutex);
	--d->reservedThreads;
	d->tryToStartMoreThreads();
}

bool ThreadPool::waitForDone(unsigned long int msec)
{
	ThreadPoolPrivate* const d = this->d_func();
	bool ret = d->waitForDone(msec);
	if (ret) {
		d->reset();
	}

	return ret;
}

void ThreadPool::clear(void)
{
	this->d_func()->clear();
}

void ThreadPool::cancel(Runnable* runnable)
{
	ThreadPoolPrivate* const d = this->d_func();
	if (!d->stealRunnable(runnable)) {
		return;
	}

	if (runnable->autoDelete() && !--runnable->m_ref) {
		delete runnable;
	}
}
