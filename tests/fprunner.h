#ifndef FPRUNNER_H
#define FPRUNNER_H

#include "../runnable.h"

typedef void (*FunctionPointer)();

class FPRunner : public Runnable {
public:
	FPRunner(FunctionPointer func) : m_func(func) {}

	virtual void run() override
	{
		this->m_func();
	}

private:
	FunctionPointer m_func;
};

static inline Runnable* createTask(FunctionPointer f)
{
	return new FPRunner(f);
}

#endif // FPRUNNER_H
