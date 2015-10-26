#ifndef RUNNABLE_H
#define RUNNABLE_H

class Runnable {
public:
	Runnable(void) : m_ref(0) {}
	virtual ~Runnable(void) noexcept {}

	bool autoDelete(void) const { return this->m_ref != -1; }
	void setAutoDelete(bool v) { this->m_ref = v ? 0 : -1; }

	virtual void run(void) = 0;
private:
	int m_ref;

	friend class ThreadPool;
	friend class ThreadPoolPrivate;
	friend class ThreadPoolThread;
};

#endif // RUNNABLE_H
