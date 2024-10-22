#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <limits>
#include <memory>

class Runnable;
class ThreadPoolPrivate;

class ThreadPool {
public:
	ThreadPool();
	~ThreadPool();

	void start(Runnable* runnable, int priority = 0);
	bool tryStart(Runnable* runnable);

	unsigned long int expiryTimeout() const;
	void setExpiryTimeout(unsigned long int v);

	std::size_t maxThreadCount() const;
	void setMaxThreadCount(std::size_t n);

	std::size_t activeThreadCount() const;
	std::size_t queueSize() const;

	void reserveThread();
	void releaseThread();

	bool waitForDone(unsigned long int timeout = std::numeric_limits<unsigned long int>::max());

	void clear();
	void cancel(Runnable* runnable);

private:
	friend class ThreadPoolPrivate;

	const std::unique_ptr<ThreadPoolPrivate> d_ptr;

	inline ThreadPoolPrivate* d_func() { return this->d_ptr.get(); }
	inline const ThreadPoolPrivate* d_func() const { return this->d_ptr.get(); }
};

#endif // THREADPOOL_H
