#ifndef COUNTINGRUNNABLE_H
#define COUNTINGRUNNABLE_H

#include <atomic>
#include "../src/runnable.h"

class CountingRunnable : public Runnable {
public:
	explicit CountingRunnable(std::atomic<int>* count) : m_cnt(count) {}

	void run() override
	{
		++(*this->m_cnt);
	}

private:
	std::atomic<int>* m_cnt;
};

#endif // COUNTINGRUNNABLE_H