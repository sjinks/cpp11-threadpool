#include <mutex>
#include <thread>
#include "threadpoolthread.h"
#include "threadpool_p.h"
#include "runnable.h"

ThreadPoolThread::ThreadPoolThread(ThreadPoolPrivate* manager)
	: manager(manager), runnable(nullptr), thread(nullptr)
{
}

void ThreadPoolThread::operator()(void)
{
	std::unique_lock<std::mutex> locker(this->manager->mutex);
	while (true) {
		Runnable* r    = this->runnable;
		this->runnable = nullptr;

		do {
			if (r) {
				const bool auto_delete = r->autoDelete();

				locker.unlock();
				r->run();
				locker.lock();

				if (auto_delete && !--r->m_ref) {
					delete r;
				}
			}

			if (this->manager->tooManyThreadsActive()) {
				break;
			}

			if (this->manager->queue.empty()) {
				r = nullptr;
			}
			else {
				r = this->manager->queue.front().first;
				this->manager->queue.pop_front();
			}
		} while (r);

		if (this->manager->isExiting) {
			this->registerThreadInactive();
			break;
		}

		bool expired = this->manager->tooManyThreadsActive();
		if (!expired) {
			this->manager->waitingThreads.push_back(this);
			this->registerThreadInactive();
			this->runnableReady.wait_for(locker, std::chrono::milliseconds(manager->expiryTimeout));
			++manager->activeThreads;

			for (auto it = this->manager->waitingThreads.begin(); it != this->manager->waitingThreads.end(); ++it) {
				if (*it == this) {
					this->manager->waitingThreads.erase(it);
					expired = true;
					break;
				}
			}
		}

		if (expired) {
			this->manager->expiredThreads.push_back(this);
			this->registerThreadInactive();
			break;
		}
	}
}

void ThreadPoolThread::registerThreadInactive(void)
{
	if (--this->manager->activeThreads == 0) {
		this->manager->noActiveThreads.notify_all();
	}
}
