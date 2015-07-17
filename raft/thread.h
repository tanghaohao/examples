#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>

class Thread {
    private:
	pthread_t id;
    public:
	Thread(): 
	    id(0) { }
	virtual ~Thread();
    protected:
	virtual void *entry() = 0;
    private:
	static void *_entry_func(void *);

    public:
	void create(size_t stacksize = 0);
	int join(void **pv = NULL);
	int kill(int signal);
	int detach();
};

#endif
