#ifndef THREADPOOLTHREAD_H
#define THREADPOOLTHREAD_H

#include <condition_variable>
#include <thread>

class Runnable;
class ThreadPoolPrivate;

class ThreadPoolThread {
public:
	explicit ThreadPoolThread(ThreadPoolPrivate* manager);

	void operator()();

	void registerThreadInactive();

	std::condition_variable runnableReady;
	ThreadPoolPrivate* manager;
	Runnable* runnable = nullptr;
	std::thread thread;
};

#endif // THREADPOOLTHREAD_H
