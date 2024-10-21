#ifndef BLOCKED_TASK_H
#define BLOCKED_TASK_H

#include <chrono>
#include <mutex>
#include <thread>
#include "../src/runnable.h"

class BlockedTask : public Runnable {
public:
	void run() override
	{
		this->lockMutex();
		this->unlockMutex();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	void lockMutex()
	{
		this->m_mutex.lock();
	}

	void unlockMutex()
	{
		this->m_mutex.unlock();
	}

private:
	std::mutex m_mutex;
};

#endif // BLOCKED_TASK_H
