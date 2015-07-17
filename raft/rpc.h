#ifndef RPC_H
#define RPC_H

#include <tr1/memory>
#include <map>
#include <deque>
#include <string>
#include "thread.h"

using std::tr1::shared_ptr;
using std::map;
using std::deque;
using std::string;

class RPCServer {
public:
    struct rpc_arg_t {
	string method_name;
	int length;
	char data[0];
    };
    typedef shared_ptr<rpc_arg_t> rpc_arg_ref;

    struct rpc_dispatch_t {
	virtual void dispatch(string, int, void *) = 0;
	rpc_dispatch_t() { }
	virtual ~rpc_dispatch_t() { }
    };
private:
    bool changed;
    map<int, rpc_dispatch_t*> dispatchers;
    map<int, deque<rpc_arg_ref> > queue;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_cond;
    void queue_arg(int, string, int, void *);

    bool stop, pause, paused;
    void entry();
    class WorkingThread: public Thread {
	RPCServer *rpc_server;
    public:
	WorkingThread(RPCServer *s) :
	    rpc_server(s) { }
	void *entry() {
	    rpc_server->entry();
	    return NULL;
	}
    } working_thread;
    void stop_working_thread();
    void pause_working_thread();
    void unpause_working_thread();

public:
    RPCServer();
    ~RPCServer();
    void register_dispatcher(int, rpc_dispatch_t *);
    void unregister_dispatcher(int);
    void call(int, string, int, void *);
};
#endif
