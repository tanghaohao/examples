#ifndef LOGEXEC_H
#define LOGEXEC_H

#include "log.h"
#include "thread.h"

class LogExecuter {
    private:
	Log *log;
	int lastApplied;
	int lastCommitted;
	pthread_mutex_t lock;
	pthread_cond_t cond;

    public:
	void update_last_committed(int c);

    private:
	virtual void do_lentry(Log::log_entry_ref) = 0;
	bool _stop;
	void exec_entry();
	class ExecThread: public Thread {
	    LogExecuter *owner;
	public:
	    ExecThread(LogExecuter *l): owner(l) { }
	    void *entry() {
		owner->exec_entry();
		return NULL;
	    }
	} engine;
    
    public:
	LogExecuter(Log *l, int a):
	    log(l), lastApplied(a), lastCommitted(0), _stop(false), engine(this) { }
	virtual ~LogExecuter();
	void start();
	void stop();
};

#endif
