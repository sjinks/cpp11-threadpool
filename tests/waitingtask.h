#ifndef WAITING_TASK_H
#define WAITING_TASK_H

#include <atomic>
#include "../runnable.h"
#include "semaphore.h"

class WaitingTask : public Runnable {
public:
	WaitingTask(std::atomic<int>* count) : m_cnt(count)
	{
		this->setAutoDelete(false);
	}

	virtual void run() override
	{
		this->m_s.acquire();
		++(*this->m_cnt);
	}

	void release(int n)
	{
		this->m_s.release(n);
	}

private:
	std::atomic<int>* m_cnt;
	semaphore m_s;
};

#endif // WAITING_TASK_H
