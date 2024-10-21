#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <gtest/gtest.h>
#include "../src/threadpool.h"

#include "blockedtask.h"
#include "countertask.h"
#include "countingrunnable.h"
#include "fprunner.h"
#include "semaphore.h"
#include "sleepertask.h"
#include "stresstest.h"
#include "waitingtask.h"

namespace {

class ThreadPoolTestSuite : public ::testing::Test {
protected:
    void SetUp() override
    {
        this->m_count.store(0, std::memory_order_relaxed);
        this->m_pool = std::make_unique<ThreadPool>();
        ThreadPoolTestSuite::g_count = 0;
    }

    void TearDown() override
    {
        this->m_pool.reset();
    }

    std::atomic<int> m_count{0};
    std::unique_ptr<ThreadPool> m_pool;
public:
    static std::mutex g_mutex;
    static int g_count;
};

std::mutex ThreadPoolTestSuite::g_mutex;
int ThreadPoolTestSuite::g_count = 0;

void empty_function()
{
    // Do nothing
}

void not_sleeping_function()
{
    ++ThreadPoolTestSuite::g_count;
}

void sleeping_function_with_mutex()
{
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::lock_guard<std::mutex> lock(ThreadPoolTestSuite::g_mutex);
    ++ThreadPoolTestSuite::g_count;
}

void not_sleeping_function_with_mutex()
{
    std::lock_guard<std::mutex> lock(ThreadPoolTestSuite::g_mutex);
    ++ThreadPoolTestSuite::g_count;
}


TEST_F(ThreadPoolTestSuite, TestRunFunction)
{
    this->m_pool->start(createTask(not_sleeping_function));
    this->m_pool->waitForDone();
    EXPECT_EQ(ThreadPoolTestSuite::g_count, 1);
}

TEST_F(ThreadPoolTestSuite, TestRunMultiple_sleeping)
{
    const auto runs = 10U;

    for (auto i = 0U; i < runs; ++i) {
        this->m_pool->start(createTask(sleeping_function_with_mutex));
    }

    this->m_pool->waitForDone();
    EXPECT_EQ(ThreadPoolTestSuite::g_count, runs);
}

TEST_F(ThreadPoolTestSuite, TestRunMultiple_not_sleeping)
{
    const auto runs = 10U;

    for (auto i = 0U; i < runs; ++i) {
        this->m_pool->start(createTask(not_sleeping_function_with_mutex));
    }

    this->m_pool->waitForDone();
    EXPECT_EQ(ThreadPoolTestSuite::g_count, runs);
}

TEST_F(ThreadPoolTestSuite, TestRunMultiple_empty)
{
    const auto runs = 500U;

    for (auto i = 0U; i < runs; ++i) {
        this->m_pool->start(createTask(empty_function));
    }

    this->m_pool->waitForDone();
}

TEST_F(ThreadPoolTestSuite, TestWaitComplete)
{
    const auto runs = 500U;
    for (auto i = 0U; i < runs; ++i) {
        ThreadPool pool;
        pool.start(createTask(not_sleeping_function));
    }

    EXPECT_EQ(ThreadPoolTestSuite::g_count, runs);
}

TEST_F(ThreadPoolTestSuite, TestRunTask)
{
    static std::atomic_bool ran;

    class TestTask : public Runnable {
    public:
        void run() override
        {
            ran.store(true, std::memory_order_relaxed);
        }
    };

    ran.store(false, std::memory_order_relaxed);
    this->m_pool->start(new TestTask());

    auto start = std::chrono::steady_clock::now();
    auto till  = start + std::chrono::seconds(5);

    while (std::chrono::steady_clock::now() < till) {
        if (ran.load(std::memory_order_relaxed)) {
            break;
        }

        std::this_thread::yield();
    }

    EXPECT_EQ(ran.load(std::memory_order_relaxed), true);
}

TEST_F(ThreadPoolTestSuite, TestDestruction)
{
    static auto value = std::make_unique<std::atomic<int>>(0);

    constexpr auto iterations = 100U;

    class IntAccessor : public Runnable {
    public:
        void run() override
        {
            for (auto i = 0U; i < iterations; ++i) {
                ++(*value.get());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    };

    {
        ThreadPool pool;
        pool.start(new IntAccessor());
        pool.start(new IntAccessor());
    }

    EXPECT_EQ(value->load(std::memory_order_relaxed), 2 * iterations);
}

TEST_F(ThreadPoolTestSuite, TestThreadRecycling)
{
    static semaphore thread_recycling_semaphore;
    static std::thread::id recycled_thread;

    class ThreadRecorderTask : public Runnable {
    public:
        void run() override
        {
            recycled_thread = std::this_thread::get_id();
            thread_recycling_semaphore.release();
        }
    };

    this->m_pool->start(new ThreadRecorderTask());
    thread_recycling_semaphore.acquire();
    auto thread1 = recycled_thread;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    this->m_pool->start(new ThreadRecorderTask());
    thread_recycling_semaphore.acquire();
    auto thread2 = recycled_thread;

    EXPECT_EQ(thread1, thread2);
    EXPECT_NE(thread1, std::thread::id());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    this->m_pool->start(new ThreadRecorderTask());
    thread_recycling_semaphore.acquire();
    auto thread3 = recycled_thread;

    EXPECT_EQ(thread3, thread2);
}

TEST_F(ThreadPoolTestSuite, TestStart)
{
    const auto runs = 1000;

    for (auto i = 0; i < runs; ++i) {
        this->m_pool->start(new CountingRunnable(&this->m_count));
    }

    this->m_pool->waitForDone();

    EXPECT_EQ(this->m_count.load(std::memory_order_relaxed), runs);
}

TEST_F(ThreadPoolTestSuite, TestTryStart)
{
    WaitingTask task(&this->m_count);
    for (auto i = 0U; i < this->m_pool->maxThreadCount(); ++i) {
        this->m_pool->start(&task);
    }

    EXPECT_FALSE(this->m_pool->tryStart(&task));
    task.release(static_cast<int>(this->m_pool->maxThreadCount()));
    this->m_pool->waitForDone();
    EXPECT_EQ(this->m_count.load(std::memory_order_relaxed), this->m_pool->maxThreadCount());
}

TEST_F(ThreadPoolTestSuite, TestTryStartPeakThreadCount)
{
    std::atomic<int> active(0);
    std::atomic<int> peak(0);

    CounterTask task(&active, &peak);

    const auto n = std::max(20U, std::thread::hardware_concurrency() * 4);
    for (auto i = 0U; i < n; ++i) {
        if (!this->m_pool->tryStart(&task)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    EXPECT_EQ(peak.load(std::memory_order_relaxed), static_cast<int>(std::thread::hardware_concurrency()));

    for (auto i = 0U; i < n; ++i) {
        if (!this->m_pool->tryStart(&task)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    EXPECT_EQ(peak.load(std::memory_order_relaxed), static_cast<int>(std::thread::hardware_concurrency()));
    this->m_pool->waitForDone();
}

TEST_F(ThreadPoolTestSuite, TestTryStartCount)
{
    SleeperTask task;

    const auto runs = 5U;

    for (auto i = 0U; i < runs; ++i) {
        auto count = 0U;
        while (this->m_pool->tryStart(&task)) {
            ++count;
        }

        EXPECT_EQ(count, std::thread::hardware_concurrency());

        auto start = std::chrono::steady_clock::now();
        auto till  = start + std::chrono::seconds(5);

        while (std::chrono::steady_clock::now() < till) {
            if (this->m_pool->activeThreadCount() == 0) {
                break;
            }

            std::this_thread::yield();
        }

        EXPECT_EQ(this->m_pool->activeThreadCount(), 0);
    }
}

TEST_F(ThreadPoolTestSuite, TestStress)
{
    auto start = std::chrono::steady_clock::now();
    auto till  = start + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < till) {
        StressTestTask t(this->m_pool.get());
        t.start();
        t.wait();
    }
}

TEST_F(ThreadPoolTestSuite, TestWaitForDone)
{
    auto start = std::chrono::steady_clock::now();
    auto till  = start + std::chrono::seconds(10);

    while (std::chrono::steady_clock::now() < till) {
        int runs = 0;
        this->m_count.store(0, std::memory_order_relaxed);
        {
            auto n = std::chrono::steady_clock::now();
            auto t = n + std::chrono::milliseconds(100);
            while (std::chrono::steady_clock::now() < t) {
                this->m_pool->start(new CountingRunnable(&this->m_count));
                ++runs;
            }
        }

        this->m_pool->waitForDone();
        EXPECT_EQ(this->m_count.load(std::memory_order_relaxed), runs);

        runs = 0;
        this->m_count.store(0, std::memory_order_relaxed);
        {
            auto n = std::chrono::steady_clock::now();
            auto t = n + std::chrono::milliseconds(100);
            while (std::chrono::steady_clock::now() < t) {
                this->m_pool->start(new CountingRunnable(&this->m_count));
                this->m_pool->start(new CountingRunnable(&this->m_count));
                runs += 2;
            }
        }

        this->m_pool->waitForDone();
        EXPECT_EQ(this->m_count.load(std::memory_order_relaxed), runs);
    }
}

TEST_F(ThreadPoolTestSuite, TestWaitForDoneTimeout)
{
    auto* task = new BlockedTask();
    task->lockMutex();
    this->m_pool->start(task);
    EXPECT_FALSE(this->m_pool->waitForDone(100));
    task->unlockMutex();
    EXPECT_TRUE(this->m_pool->waitForDone(500));
}

TEST_F(ThreadPoolTestSuite, TestDestroyingWaitsForTasksToFinish)
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
        EXPECT_EQ(this->m_count.load(std::memory_order_relaxed), runs);

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
        EXPECT_EQ(this->m_count.load(std::memory_order_relaxed), runs);
    }
}

} // namespace
