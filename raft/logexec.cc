#include "logexec.h"

void LogExecuter::update_last_committed(int c)
{
    if (lastCommitted >= c)
	return;
    pthread_mutex_lock(&lock);
    lastCommitted = c;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}

void LogExecuter::exec_entry()
{
    int committed = lastCommitted;
    while(!_stop) {
	while(lastApplied <= committed) {
	    Log::log_entry_ref e = log->get(lastApplied+1);
	    do_lentry(e);
	    ++lastApplied;
	}
	lastApplied = committed;

	pthread_mutex_lock(&lock);
	while(lastCommitted == committed && !_stop)
	    pthread_cond_wait(&cond, &lock);
	committed = lastCommitted;
	pthread_mutex_unlock(&lock);
    }
}

LogExecuter::~LogExecuter()
{
    stop();
}

void LogExecuter::start()
{
    _stop = false;
    engine.create();
}

void LogExecuter::stop()
{
    if (_stop)
	return;
    pthread_mutex_lock(&lock);
    _stop = true;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}
