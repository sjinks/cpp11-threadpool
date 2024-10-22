#include <algorithm>
#include <chrono>
#include <thread>
#include "threadpool_p.h"
#include "threadpoolthread.h"
#include "runnable.h"

ThreadPoolPrivate::ThreadPoolPrivate()
	: maxThreadCount(std::max(std::thread::hardware_concurrency(), 1u))
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
		auto* t = this->waitingThreads.front();
		this->waitingThreads.pop_front();
		t->runnableReady.notify_one();
		return true;
	}

	if (this->expiredThreads.size()) {
		auto* t = this->expiredThreads.front();
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

std::size_t ThreadPoolPrivate::activeThreadCount() const
{
	return
		  this->allThreads.size()
		- this->waitingThreads.size()
		- this->expiredThreads.size()
		+ this->reservedThreads
	;
}

void ThreadPoolPrivate::tryToStartMoreThreads()
{
	while (!this->queue.empty() && this->tryStart(this->queue.front().first)) {
		this->queue.pop_front();
	}
}

bool ThreadPoolPrivate::tooManyThreadsActive() const
{
	const auto activeThreadCount = this->activeThreadCount();
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

void ThreadPoolPrivate::reset()
{
	std::unique_lock<std::mutex> locker(this->mutex);
	this->isExiting = true;

	while (!this->allThreads.empty()) {
		std::set<ThreadPoolThread*> allThreadsCopy;
		allThreadsCopy.swap(this->allThreads);
		locker.unlock();

		for (auto it : allThreadsCopy) {
			it->runnableReady.notify_all();
			it->thread.join();
			delete it;
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
		this->noActiveThreads.wait(locker, [this] {
			return this->queue.empty() && this->activeThreads == 0;
		});
	}
	else {
		const auto duration = std::chrono::milliseconds(msecs);

		this->noActiveThreads.wait_for(locker, duration, [this] {
			return this->queue.empty() && this->activeThreads == 0;
		});
	}

	return this->queue.empty() && !this->activeThreads;
}

void ThreadPoolPrivate::clear()
{
	std::unique_lock<std::mutex> locker(this->mutex);
	while (!this->queue.empty()) {
		auto& item = this->queue.front();
		auto* r = item.first;
		if (r->autoDelete() && --r->m_ref) {
			delete r;
		}

		this->queue.pop_front();
	}
}

bool ThreadPoolPrivate::stealRunnable(const Runnable* runnable)
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
	if (this->stealRunnable(runnable)) {
		const auto autoDelete = runnable->autoDelete();
		bool del = autoDelete && !--runnable->m_ref;

		runnable->run();

		if (del) {
			delete runnable;
		}
	}
}
