#include <chrono>
#include <mutex>
#include <thread>
#include "testsuite.h"

int g_count = 0;
std::mutex g_mutex;

void sleepingFunction()
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	++g_count;
}

void emptyFunction()
{
}

void notSleepingFunction()
{
	++g_count;
}

void sleepingFunctionWithMutex()
{
	std::this_thread::sleep_for(std::chrono::seconds(1));
	g_mutex.lock();
	++g_count;
	g_mutex.unlock();
}

void notSleepingFunctionWithMutex()
{
	g_mutex.lock();
	++g_count;
	g_mutex.unlock();
}
