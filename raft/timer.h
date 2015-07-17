#ifndef TIMER_H
#define TIMER_H

#include <math.h>
#include <sys/time.h>
#include <stdint.h>
#include <iostream>
#include <map>
#include "thread.h"

struct utime_t {
    uint32_t sec;
    uint32_t nsec;
    utime_t(double s) {
	sec = trunc(s);
	nsec = (s - sec) * 1000000000L;
    }
    utime_t(const struct timeval &v) {
	sec = v.tv_sec;
	nsec = v.tv_usec*1000;
    }
    struct timespec to_timespec() const {
	timespec t;
	t.tv_sec = sec;
	t.tv_nsec = nsec;
	return t;
    }
};

inline bool operator>(const utime_t& a, const utime_t& b) {
    return a.sec>b.sec || (a.sec==b.sec && a.nsec>b.nsec);
}

inline bool operator<(const utime_t &a, const utime_t& b) {
    return a.sec<b.sec || (a.sec==b.sec && a.nsec<b.nsec);
}

inline bool operator>=(const utime_t &a, const utime_t& b) {
    return !(operator<(a, b));
} 

inline bool operator<=(const utime_t &a, const utime_t& b) {
    return !(operator>(a, b));
}

inline bool operator==(const utime_t &a, const utime_t& b) {
    return a.sec==b.sec && a.nsec==b.nsec;
}    

utime_t get_current_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return utime_t(t);
}

class Context {
    public:
	Context() { }
	virtual ~Context() { }
	void complete() {
	    finish();
	    delete this;
	}
    private:
	virtual void finish() = 0;
};

class Timer {
    std::multimap<utime_t, Context*> scheduled_map;
    std::map<Context*, std::multimap<utime_t, Context*>::iterator> events;
    pthread_mutex_t lock;
    pthread_cond_t cond;

    bool stop;
    void tick();
    class TimeThread: public Thread {
	Timer *owner;
    public:
	TimeThread(Timer *t): owner(t) { }
	void *entry() {
	    owner->tick();
	    return NULL;
	}
    } time_thread;
 public:
    Timer();
    void start_tick();
    void stop_tick();
    void add_event_at(utime_t &, Context*);
    void add_event_after(double second, Context*);
    Context* cancel_event(Context*);
};
#endif
