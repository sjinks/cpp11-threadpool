#ifndef THREADPOOL_TESTSUITE_H
#define THREADPOOL_TESTSUITE_H

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <cxxtest/TestSuite.h>
#include "../threadpool.h"
#include "../runnable.h"
#include "blockedtask.h"
#include "countertask.h"
#include "countingrunnable.h"
#include "fprunner.h"
#include "semaphore.h"
#include "sleepertask.h"
#include "stresstest.h"
#include "waitingtask.h"

extern int g_count;
extern std::mutex g_mutex;
void sleepingFunction();
void emptyFunction();
void notSleepingFunction();
void sleepingFunctionWithMutex();
void notSleepingFunctionWithMutex();

class ThreadPoolTestSuite : public CxxTest::TestSuite {
public:
	ThreadPoolTestSuite() : m_count(0), m_pool(new ThreadPool) {}
	~ThreadPoolTestSuite() { delete this->m_pool; }

	void testRunFunction()
	{
		{
			ThreadPool pool;
			g_count = 0;
			pool.start(createTask(notSleepingFunction));
		}

		TS_ASSERT_EQUALS(g_count, 1);
	}

	void testRunMultiple()
	{
		const int runs = 10;

		{
			ThreadPool pool;
			g_count = 0;
			for (int i=0; i<runs; ++i) {
				pool.start(createTask(sleepingFunctionWithMutex));
			}
		}
		TS_ASSERT_EQUALS(g_count, runs);

		{
			ThreadPool pool;
			g_count = 0;
			for (int i=0; i<runs; ++i) {
				pool.start(createTask(notSleepingFunctionWithMutex));
			}
		}
		TS_ASSERT_EQUALS(g_count, runs);

		{
			ThreadPool pool;
			for (int i=0; i<500; ++i) {
				pool.start(createTask(emptyFunction));
			}
		}
	}

	void testWaitComplete()
	{
		g_count = 0;
		const int runs = 500;
		for (int i=0; i<runs; ++i) {
			ThreadPool pool;
			pool.start(createTask(notSleepingFunction));
		}

		TS_ASSERT_EQUALS(g_count, runs);
	}

	void testRunTask()
	{
		static std::atomic<bool> ran;

		class TestTask : public Runnable {
		public:
			virtual void run() override
			{
				ran.store(true, std::memory_order_relaxed);
			}
		};

		ran.store(false, std::memory_order_relaxed);
		ThreadPool pool;
		pool.start(new TestTask);

		auto start = std::chrono::steady_clock::now();
		auto till  = start + std::chrono::seconds(5);

		while (std::chrono::steady_clock::now() < till) {
			if (ran.load(std::memory_order_relaxed)) {
				break;
			}

			std::this_thread::yield();
		}

		TS_ASSERT_EQUALS(ran.load(std::memory_order_relaxed), true);
	}

	void testDestruction()
	{
		static std::atomic<int>* value = nullptr;

		class IntAccessor : public Runnable {
		public:
			virtual void run() override
			{
				for (int i=0; i<100; ++i) {
					++(*value);
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
			}
		};

		value = new std::atomic<int>;
		value->store(0, std::memory_order_relaxed);
		ThreadPool* pool = new ThreadPool;
		pool->start(new IntAccessor());
		pool->start(new IntAccessor());
		delete pool;
		TS_ASSERT_EQUALS(value->load(std::memory_order_relaxed), 200);
		delete value;
	}

	void testThreadRecycling()
	{
		static semaphore threadRecyclingSemaphore;
		static std::thread::id recycledThread;

		class ThreadRecorderTask : public Runnable {
		public:
			virtual void run() override
			{
				recycledThread = std::this_thread::get_id();
				threadRecyclingSemaphore.release();
			}
		};

		ThreadPool pool;

		pool.start(new ThreadRecorderTask);
		threadRecyclingSemaphore.acquire();
		std::thread::id thread1 = recycledThread;

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		pool.start(new ThreadRecorderTask);
		threadRecyclingSemaphore.acquire();
		std::thread::id thread2 = recycledThread;
		TS_ASSERT_EQUALS(thread1, thread2);
		TS_ASSERT_DIFFERS(thread1, std::thread::id());

		std::this_thread::sleep_for(std::chrono::milliseconds(100));

		pool.start(new ThreadRecorderTask);
		threadRecyclingSemaphore.acquire();
		std::thread::id thread3 = recycledThread;
		TS_ASSERT_EQUALS(thread3, thread2);
	}

	void testStart()
	{
		const int runs = 1000;
		this->m_count.store(0, std::memory_order_relaxed);
		{
			ThreadPool threadPool;
			for (int i=0; i<runs; ++i) {
				threadPool.start(new CountingRunnable(&this->m_count));
			}
		}
		TS_ASSERT_EQUALS(this->m_count.load(std::memory_order_relaxed), runs);
	}

	void testTryStart()
	{
		this->m_count.store(0, std::memory_order_relaxed);
		WaitingTask task(&this->m_count);
		ThreadPool pool;
		for (std::size_t i=0; i<pool.maxThreadCount(); ++i) {
			pool.start(&task);
		}

		TS_ASSERT(!pool.tryStart(&task))
		task.release(pool.maxThreadCount());
		pool.waitForDone();
		TS_ASSERT_EQUALS(this->m_count.load(std::memory_order_relaxed), pool.maxThreadCount());
	}

	void testTryStartPeakThreadCount()
	{
		std::atomic<int> active;
		std::atomic<int> peak;

		active.store(0, std::memory_order_relaxed);
		peak.store(0, std::memory_order_relaxed);

		CounterTask task(&active, &peak);
		ThreadPool pool;

		int n = std::max(20u, std::thread::hardware_concurrency() * 4);
		for (int i=0; i<n; ++i) {
			if (pool.tryStart(&task) == false) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

		TS_ASSERT_EQUALS(peak.load(std::memory_order_relaxed), std::thread::hardware_concurrency());

		for (int i=0; i<n; ++i) {
			if (pool.tryStart(&task) == false) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

		TS_ASSERT_EQUALS(peak.load(std::memory_order_relaxed), std::thread::hardware_concurrency());
	}

	void testTryStartCount()
	{
		SleeperTask task;
		ThreadPool pool;
		const int runs = 5;

		for (int i=0; i<runs; ++i) {
			int count = 0;
			while (pool.tryStart(&task)) {
				++count;
			}

			TS_ASSERT_EQUALS(count, std::thread::hardware_concurrency());

			auto start = std::chrono::steady_clock::now();
			auto till  = start + std::chrono::seconds(5);

			while (std::chrono::steady_clock::now() < till) {
				if (pool.activeThreadCount() == 0) {
					break;
				}

				std::this_thread::yield();
			}

			TS_ASSERT_EQUALS(pool.activeThreadCount(), 0);
		}
	}

	void testStress()
	{
		auto start = std::chrono::steady_clock::now();
		auto till  = start + std::chrono::seconds(5);
		while (std::chrono::steady_clock::now() < till) {
			StressTestTask t(this->m_pool);
			t.start();
			t.wait();
		}
	}

	void testWaitForDone()
	{
		ThreadPool pool;
		auto start = std::chrono::steady_clock::now();
		auto till  = start + std::chrono::seconds(10);
		while (std::chrono::steady_clock::now() < till) {
			int runs = 0;
			this->m_count.store(0, std::memory_order_relaxed);
			{
				auto n = std::chrono::steady_clock::now();
				auto t = n + std::chrono::milliseconds(100);
				while (std::chrono::steady_clock::now() < t) {
					pool.start(new CountingRunnable(&this->m_count));
					++runs;
				}
			}

			pool.waitForDone();
			TS_ASSERT_EQUALS(this->m_count.load(std::memory_order_relaxed), runs);

			runs = 0;
			this->m_count.store(0, std::memory_order_relaxed);
			{
				auto n = std::chrono::steady_clock::now();
				auto t = n + std::chrono::milliseconds(100);
				while (std::chrono::steady_clock::now() < t) {
					pool.start(new CountingRunnable(&this->m_count));
					pool.start(new CountingRunnable(&this->m_count));
					runs += 2;
				}
			}

			pool.waitForDone();
			TS_ASSERT_EQUALS(this->m_count.load(std::memory_order_relaxed), runs);
		}
	}

	void testWaitForDoneTimeout()
	{
		ThreadPool pool;
		BlockedTask* task = new BlockedTask();
		task->lockMutex();
		pool.start(task);
		TS_ASSERT(!pool.waitForDone(100));
		task->unlockMutex();
		TS_ASSERT(pool.waitForDone(500));
	}

	void testDestroyingWaitsForTasksToFinish()
	{
		auto start = std::chrono::steady_clock::now();
		auto till  = start + std::chrono::seconds(10);
		while (std::chrono::steady_clock::now() < till) {
			int runs = 0;
			this->m_count.store(0, std::memory_order_relaxed);
			{
				ThreadPool pool;
				auto n = std::chrono::steady_clock::now();
				auto t = n + std::chrono::milliseconds(100);
				while (std::chrono::steady_clock::now() < t) {
					pool.start(new CountingRunnable(&this->m_count));
					++runs;
				}
			}
			TS_ASSERT_EQUALS(this->m_count.load(std::memory_order_relaxed), runs);

			runs = 0;
			this->m_count.store(0, std::memory_order_relaxed);
			{
				ThreadPool pool;
				auto n = std::chrono::steady_clock::now();
				auto t = n + std::chrono::milliseconds(100);
				while (std::chrono::steady_clock::now() < t) {
					pool.start(new CountingRunnable(&this->m_count));
					pool.start(new CountingRunnable(&this->m_count));
					runs += 2;
				}
			}
			TS_ASSERT_EQUALS(this->m_count.load(std::memory_order_relaxed), runs);
		}
	}

	void testSetMaxThreadCount()
	{
		std::vector<int> values({1, -1, 2, -2, 4, -4, 0, 12345, -6789, 42, -666});
		for (int limit : values) {
			int savedLimit = this->m_pool->maxThreadCount();

			// maxThreadCount() should always return the previous argument to
			// setMaxThreadCount(), regardless of input
			this->m_pool->setMaxThreadCount(limit);
			TS_ASSERT_EQUALS(this->m_pool->maxThreadCount(), limit);

			// the value returned from maxThreadCount() should always be valid input for setMaxThreadCount()
			this->m_pool->setMaxThreadCount(savedLimit);
			TS_ASSERT_EQUALS(this->m_pool->maxThreadCount(), savedLimit);
		}
	}

	

private:
	std::atomic<int> m_count;
	ThreadPool* m_pool;
};

#endif // THREADPOOL_TESTSUITE_H
