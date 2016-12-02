#ifndef SLEEPER_TASK_H
#define SLEEPER_TASK_H

#include <chrono>
#include "../runnable.h"

class SleeperTask : public Runnable {
public:
	SleeperTask()
	{
		this->setAutoDelete(false);
	}

	virtual void run() override
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
};

#endif // SLEEPER_TASK_H
