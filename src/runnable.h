#ifndef RUNNABLE_H
#define RUNNABLE_H

class Runnable {
public:
	virtual ~Runnable(void) = default;

	bool autoDelete() const noexcept { return this->m_ref != -1; }
	void setAutoDelete(bool v) noexcept { this->m_ref = v ? 0 : -1; }

	virtual void run() = 0;
private:
	int m_ref = 0;

	friend class ThreadPool;
	friend class ThreadPoolPrivate;
	friend class ThreadPoolThread;
};

#endif // RUNNABLE_H
