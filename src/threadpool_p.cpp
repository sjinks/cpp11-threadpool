#include <algorithm>
#include <chrono>
#include <thread>
#include "threadpool_p.h"
#include "threadpoolthread.h"
#include "runnable.h"

ThreadPoolPrivate::ThreadPoolPrivate(void)
	: isExiting(false), expiryTimeout(30000),
	  maxThreadCount(std::max(std::thread::hardware_concurrency(), 1u)),
	  reservedThreads(0), activeThreads(0)
{
}

bool ThreadPoolPrivate::tryStart(Runnable* task)
{
	if (this->allThreads.empty()) {
		this->startThread(task);
		return true;
	}

	if (this->activeThreadCount() >= this->maxThreadCount) {
		return false;
	}

	if (this->waitingThreads.size()) {
		this->enqueueTask(task);
		ThreadPoolThread* t = this->waitingThreads.front();
		this->waitingThreads.pop_front();
		t->runnableReady.notify_one();
		return true;
	}

	if (this->expiredThreads.size()) {
		ThreadPoolThread* t = this->expiredThreads.front();
		this->expiredThreads.pop_front();

		++this->activeThreads;

		if (task->autoDelete()) {
			++task->m_ref;
		}

		t->runnable = task;
		t->thread.join();
		t->thread = std::thread(&ThreadPoolThread::operator(), t);
		return true;
	}

	this->startThread(task);
	return true;
}

inline bool operator<(int priority, const std::pair<Runnable*, int>& p)
{
	return p.second < priority;
}

inline bool operator<(const std::pair<Runnable*, int>&p, int priority)
{
	return priority < p.second;
}

void ThreadPoolPrivate::enqueueTask(Runnable* runnable, int priority)
{
	if (runnable->autoDelete()) {
		++runnable->m_ref;
	}

	auto it = std::upper_bound(this->queue.begin(), this->queue.end(), priority);
	this->queue.insert(it, std::make_pair(runnable, priority));
}

std::size_t ThreadPoolPrivate::activeThreadCount(void) const
{
	return
		  this->allThreads.size()
		- this->waitingThreads.size()
		- this->expiredThreads.size()
		+ this->reservedThreads
	;
}

void ThreadPoolPrivate::tryToStartMoreThreads(void)
{
	while (!this->queue.empty() && this->tryStart(this->queue.front().first)) {
		this->queue.pop_front();
	}
}

bool ThreadPoolPrivate::tooManyThreadsActive(void) const
{
	const std::size_t activeThreadCount = this->activeThreadCount();
	return activeThreadCount > this->maxThreadCount && (activeThreadCount - this->reservedThreads) > 1;
}

void ThreadPoolPrivate::startThread(Runnable* runnable)
{
	std::unique_ptr<ThreadPoolThread> thread(new ThreadPoolThread(this));
	this->allThreads.insert(thread.get());
	++this->activeThreads;

	if (runnable->autoDelete()) {
		++runnable->m_ref;
	}

	thread->runnable = runnable;
	thread->thread   = std::thread(&ThreadPoolThread::operator(), thread.get());
	thread.release();
}

void ThreadPoolPrivate::reset(void)
{
	std::unique_lock<std::mutex> locker(this->mutex);
	this->isExiting = true;

	while (!this->allThreads.empty()) {
		std::set<ThreadPoolThread*> allThreadsCopy;
		allThreadsCopy.swap(this->allThreads);
		locker.unlock();

		for (auto it = allThreadsCopy.begin(); it != allThreadsCopy.end(); ++it) {
			(*it)->runnableReady.notify_all();
			(*it)->thread.join();
			delete (*it);
		}

		locker.lock();
	}

	this->waitingThreads.clear();
	this->expiredThreads.clear();
	isExiting = false;
}

bool ThreadPoolPrivate::waitForDone(unsigned long int msecs)
{
	std::unique_lock<std::mutex> locker(this->mutex);
	if (msecs == std::numeric_limits<unsigned long int>::max()) {
		while (!(this->queue.empty() && this->activeThreads == 0)) {
			this->noActiveThreads.wait(locker);
		}
	}
	else {
		auto start = std::chrono::steady_clock::now();
		auto till  = start + std::chrono::milliseconds(msecs);
		while (
			   !(this->queue.empty() && this->activeThreads == 0)
			&& std::chrono::steady_clock::now() < till
		) {
			this->noActiveThreads.wait_until(locker, till);
		}
	}

	return this->queue.empty() && !this->activeThreads;
}

void ThreadPoolPrivate::clear(void)
{
	std::unique_lock<std::mutex> locker(this->mutex);
	while (!this->queue.empty()) {
		std::pair<Runnable*, int>& item = this->queue.front();
		Runnable* r = item.first;
		if (r->autoDelete() && --r->m_ref) {
			delete r;
		}

		this->queue.pop_front();
	}
}

bool ThreadPoolPrivate::stealRunnable(Runnable* runnable)
{
	if (!runnable) {
		return false;
	}

	std::unique_lock<std::mutex> locker(this->mutex);
	auto it  = this->queue.begin();
	auto end = this->queue.end();

	while (it != end) {
		if (it->first == runnable) {
			this->queue.erase(it);
			return true;
		}

		++it;
	}

	return false;
}

void ThreadPoolPrivate::stealAndRunRunnable(Runnable* runnable)
{
	if (!this->stealRunnable(runnable)) {
		return;
	}

	const bool autoDelete = runnable->autoDelete();
	bool del = autoDelete && !--runnable->m_ref;

	runnable->run();

	if (del) {
		delete runnable;
	}
}
