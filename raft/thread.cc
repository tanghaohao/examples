#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "assert.h"
#include "thread.h"

void *Thread::_entry_func(void *param)
{
    Thread *owner = (Thread*)param;
    return owner->entry();
}

void Thread::create(size_t stacksize)
{
    pthread_attr_t *attr = NULL;
    if (stacksize) {
	attr = (pthread_attr_t*) malloc(sizeof(pthread_attr_t));
	if (attr) {
	    pthread_attr_init(attr);
	    pthread_attr_setstacksize(attr, stacksize);
	}
    }
    int r = pthread_create(&id, attr, _entry_func, (void*)this);
    assert( r == 0);
}

int Thread::join(void **pval)
{
    if (!id)
	return -EINVAL;
    int status = pthread_join(id, pval);
    assert(status == 0);
    id = 0;
    return status;
}

int Thread::kill(int signal)
{
    if (id)
	return pthread_kill(id, signal);
    else
	-EINVAL;
}

int Thread::detach()
{
    return pthread_detach(id);
}

