#ifndef STRESSTEST_H
#define STRESSTEST_H

#include "../runnable.h"
#include "../threadpool.h"
#include "semaphore.h"

class StressTestTask : public Runnable {
public:
	StressTestTask(ThreadPool* pool)
		: m_pool(pool)
	{
		this->setAutoDelete(false);
	}

	void start()
	{
		this->m_pool->start(this);
	}

	void wait()
	{
		this->m_s.acquire();
	}

	virtual void run() override
	{
		this->m_s.release();
	}

private:
	ThreadPool* m_pool;
	semaphore m_s;
};

#endif // STRESSTEST_H
