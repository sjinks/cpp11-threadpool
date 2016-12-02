#ifndef COUNTINGRUNNABLE_H
#define COUNTINGRUNNABLE_H

#include <atomic>
#include "../runnable.h"

class CountingRunnable : public Runnable {
public:
	CountingRunnable(std::atomic<int>* count) : m_cnt(count) {}

	virtual void run() override
	{
		++(*this->m_cnt);
	}

private:
	std::atomic<int>* m_cnt;
};

#endif // COUNTINGRUNNABLE_H