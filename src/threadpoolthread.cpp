#include <algorithm>
#include <mutex>
#include "threadpoolthread.h"
#include "threadpool_p.h"
#include "runnable.h"

ThreadPoolThread::ThreadPoolThread(ThreadPoolPrivate* manager)
	: manager(manager)
{
}

void ThreadPoolThread::operator()(void)
{
	std::unique_lock<std::mutex> locker(this->manager->mutex);
	while (true) {
		auto* r        = this->runnable;
		this->runnable = nullptr;

		do {
			if (r) {
				const auto auto_delete = r->autoDelete();

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

			auto it = std::find(this->manager->waitingThreads.begin(), this->manager->waitingThreads.end(), this);
			if (it != this->manager->waitingThreads.end()) {
				this->manager->waitingThreads.erase(it);
				expired = true;
			}
		}

		if (expired) {
			this->manager->expiredThreads.push_back(this);
			this->registerThreadInactive();
			break;
		}
	}
}

void ThreadPoolThread::registerThreadInactive()
{
	if (--this->manager->activeThreads == 0) {
		this->manager->noActiveThreads.notify_all();
	}
}
