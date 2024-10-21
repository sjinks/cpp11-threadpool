#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <cassert>
#include <condition_variable>
#include <mutex>

class semaphore {
public:
	explicit semaphore(int n = 0)
		: m_cnt(n)
	{
		assert(n >= 0);
	}

	void acquire(int n = 1)
	{
		assert(n >= 0);
		std::unique_lock<std::mutex> lock(this->m_mutex);
		this->m_cv.wait(lock, [this, n] { return n <= this->m_cnt; });
		this->m_cnt -= n;
	}

	void release(int n = 1)
	{
		assert(n >= 0);
		std::unique_lock<std::mutex> lock(this->m_mutex);
		this->m_cnt += n;
		if (n > 1) {
			this->m_cv.notify_all();
		}
		else {
			this->m_cv.notify_one();
		}
	}

	int available()
	{
		std::unique_lock<std::mutex> lock(this->m_mutex);
		return this->m_cnt;
	}

	bool try_acquire(int n = 1)
	{
		assert(n >= 0);
		std::unique_lock<std::mutex> lock(this->m_mutex);
		if (n > this->m_cnt) {
			return false;
		}

		this->m_cnt -= n;
		return true;
	}

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	int m_cnt;
};

#endif // SEMAPHORE_H
