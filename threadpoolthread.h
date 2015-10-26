#ifndef THREADPOOLTHREAD_H
#define THREADPOOLTHREAD_H

#include <condition_variable>

class Runnable;
class ThreadPoolPrivate;

namespace std {
	class thread;
}

class ThreadPoolThread {
public:
	ThreadPoolThread(ThreadPoolPrivate* manager);

	void operator()(void);

	void registerThreadInactive(void);

	std::condition_variable runnableReady;
	ThreadPoolPrivate* manager;
	Runnable* runnable;
	std::thread* thread;
};

#endif // THREADPOOLTHREAD_H
