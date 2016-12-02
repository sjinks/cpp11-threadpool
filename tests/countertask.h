#ifndef COUNTER_TASK_H
#define COUNTER_TASK_H

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include "../runnable.h"

class CounterTask : public Runnable {
public:
	CounterTask(std::atomic<int>* active, std::atomic<int>* peak)
		: m_active(active), m_peak(peak)
	{
		this->setAutoDelete(false);
	}

	virtual void run() override
	{
		{
			std::unique_lock<std::mutex> lock(this->m_mutex);
			++(*this->m_active);
			this->m_peak->store(std::max(this->m_peak->load(std::memory_order_relaxed), this->m_active->load(std::memory_order_relaxed)), std::memory_order_relaxed);
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		{
			std::unique_lock<std::mutex> lock(this->m_mutex);
			--(*this->m_active);
		}
	}

private:
	std::atomic<int>* m_active;
	std::atomic<int>* m_peak;
	std::mutex m_mutex;
};

#endif // COUNTER_TASK_H
