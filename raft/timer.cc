#include <iostream>
#include "timer.h"
#include "assert.h"

Timer::Timer() :
    time_thread(this)
{
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    stop = false;
}

void Timer::start_tick()
{
    stop = false;
    time_thread.create();
}

void Timer::stop_tick()
{
    stop = true;
    pthread_mutex_lock(&lock);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
    time_thread.join();
}

void Timer::tick()
{
    pthread_mutex_lock(&lock);
    while(!stop) {
	while(scheduled_map.empty() && !stop)
	    pthread_cond_wait(&cond, &lock);
	if (stop)
	    break;
	utime_t now = get_current_time();
	std::multimap<utime_t, Context*>::iterator it = scheduled_map.begin();
	if (it->first > now) {
	    struct timespec until = it->first.to_timespec();
	    pthread_cond_timedwait(&cond, &lock, &until);
	    continue;
	}
	Context *cb = it->second;
	events.erase(cb);
	scheduled_map.erase(it);
	pthread_mutex_unlock(&lock);
	cb->complete();
	pthread_mutex_lock(&lock);
    }
    pthread_mutex_unlock(&lock);
}

void Timer::add_event_at(utime_t &t, Context *cb)
{
    pthread_mutex_lock(&lock);
    std::multimap<utime_t, Context*>::value_type value(t, cb);
    std::multimap<utime_t, Context*>::iterator it =  scheduled_map.insert(value);
    events[cb] = it;
    if (it == scheduled_map.begin())
	pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}

void Timer::add_event_after(double second, Context *cb)
{
    utime_t now = get_current_time();
    utime_t when(second);
    when.nsec += now.nsec;
    when.sec += when.nsec / 1000000000L;
    when.nsec %= 1000000000;
    when.sec += now.sec;
    add_event_at(when, cb); 
}

Context *Timer::cancel_event(Context *cb)
{
    pthread_mutex_lock(&lock);
    std::map<Context*,
	std::multimap<utime_t, Context*>::iterator>::iterator it = events.find(cb);
    if (it==events.end())
	return NULL;
    scheduled_map.erase(it->second);
    events.erase(it);
    return cb;
}
