#ifndef FPRUNNER_H
#define FPRUNNER_H

#include "../src/runnable.h"

using function_pointer = void (*)();

class FPRunner : public Runnable {
public:
	explicit FPRunner(function_pointer func) : m_func(func) {}

	void run() override
	{
		this->m_func();
	}

private:
	function_pointer m_func;
};

static inline Runnable* createTask(function_pointer f)
{
	return new FPRunner(f);
}

#endif // FPRUNNER_H
