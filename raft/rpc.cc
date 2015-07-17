#include <stdlib.h>
#include <string.h>
#include "rpc.h"

void RPCServer::queue_arg(int id, string method, int len, void *data)
{
   int size = sizeof(rpc_arg_t) + len;
   rpc_arg_t *p = (rpc_arg_t*)malloc(size);
   p->method_name = method;
   p->length = len;
   memcpy(p->data, data, len);
   pthread_mutex_lock(&queue_lock);
   queue[id].push_back(rpc_arg_ref(p));
   pthread_cond_signal(&queue_cond);
   pthread_mutex_unlock(&queue_lock);
}

void RPCServer::entry()
{
    pthread_mutex_lock(&queue_lock);
    map<int, deque<rpc_arg_ref> >::iterator it = queue.begin();
    while(!stop) {
	int size = queue.size();
	int count = 0;
	if (changed) {
	    it = queue.begin();
	    changed = false;
	}
	while(count < size) {
	    if (it == queue.end())
		it = queue.begin();
	    ++ count;
	    if (!it->second.empty())
		break;
	    ++ it;
	}
	if (it->second.empty()) {
	    pthread_cond_wait(&queue_cond, &queue_lock);
	    continue;
	}
	int id = it->first;
	rpc_arg_ref arg = it->second.front();
	if (dispatchers.count(id) == 0) {
	    queue.erase(it);
	    continue;
	}
	rpc_dispatch_t *inst = dispatchers[id];
	
	pthread_mutex_unlock(&queue_lock);
	inst->dispatch(arg->method_name, arg->length, arg->data);
	pthread_mutex_lock(&queue_lock);
	it->second.pop_front();
	++ it;
	while(pause) {
	    paused = true;
	    pthread_cond_wait(&queue_cond, &queue_lock);
	}
	paused = false;
    }
    pthread_mutex_unlock(&queue_lock);
}

void RPCServer::stop_working_thread()
{
    if (stop)
	return;
    pthread_mutex_lock(&queue_lock);
    stop = true;
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_lock);
}

void RPCServer::pause_working_thread()
{
    if (paused)
	return;
    pause = true;
    pthread_mutex_lock(&queue_lock);
    while(!paused)
	pthread_cond_wait(&queue_cond, &queue_lock);
    pthread_mutex_unlock(&queue_lock);
}

void RPCServer::unpause_working_thread()
{
    pthread_mutex_lock(&queue_lock);
    pause = false;
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_lock);
}

void RPCServer::register_dispatcher(int id, rpc_dispatch_t *inst)
{
    pthread_mutex_lock(&queue_lock);
    dispatchers[id] = inst;
    changed = true;
    pthread_mutex_unlock(&queue_lock);
}

void RPCServer::unregister_dispatcher(int id)
{
    pause_working_thread();
    dispatchers.erase(id);
    changed = true;
    unpause_working_thread();
}

void RPCServer::call(int id, string method, int len, void *data)
{
    queue_arg(id, method, len, data);
}

RPCServer::RPCServer():
    changed(false),
    stop(false),
    pause(false),
    paused(false),
    working_thread(this)
{
    pthread_mutex_init(&queue_lock, NULL);
    pthread_cond_init(&queue_cond, NULL);
}

RPCServer::~RPCServer() {
    stop_working_thread();
}
