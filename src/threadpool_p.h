#ifndef THREADPOOL_P_H
#define THREADPOOL_P_H

#include <condition_variable>
#include <list>
#include <mutex>
#include <set>
#include <utility>

class Runnable;
class ThreadPoolThread;

class ThreadPoolPrivate {
public:
	ThreadPoolPrivate();

	bool tryStart(Runnable* runnable);
	void enqueueTask(Runnable* runnable, int priority = 0);

	std::size_t activeThreadCount() const;

	void tryToStartMoreThreads();
	bool tooManyThreadsActive() const;

	void startThread(Runnable* runnable = nullptr);
	void reset();
	bool waitForDone(unsigned long int msecs);
	void clear();
	bool stealRunnable(const Runnable* runnable);
	void stealAndRunRunnable(Runnable* runnable);

	mutable std::mutex mutex;
	std::set<ThreadPoolThread*> allThreads;
	std::list<ThreadPoolThread*> waitingThreads;
	std::list<ThreadPoolThread*> expiredThreads;
	std::list<std::pair<Runnable*, int> > queue;
	std::condition_variable noActiveThreads;

	bool isExiting = false;
	unsigned long int expiryTimeout = 30000;
	std::size_t maxThreadCount;
	std::size_t reservedThreads = 0;
	std::size_t activeThreads = 0;
};

#endif // THREADPOOL_P_H
