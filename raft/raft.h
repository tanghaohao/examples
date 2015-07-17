#ifndef _RAFT_H_
#define _RAFT_H_

#include <boost/statechart/event.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/state.hpp>
#include <boost/statechart/transition.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <tr1/memory>
#include <iostream>
#include <set>
#include <deque>
#include "rpc.h"
#include "log.h"
#include "logexec.h"
#include "request.h"
#include "timer.h"
#include "thread.h"

using namespace std;
using std::tr1::shared_ptr;

set<int> peers;

namespace sc = boost::statechart;

class Raft: public RPCServer::rpc_dispatch_t {
    int uuid;
    int currentTerm;
    int votedFor;
    Log *log;

    int commitIndex;
    int lastApllied;
    int leader;

    /* event queue */
    struct EventType { 
	static const int TimeOut_t = 1;
	static const int RPC_t = 2;
	static const int Vote_t = 3;
	static const int Response_t = 4;
	static const int ClientReq_t = 5;
	int type;
	EventType(int t): type(t) { }
	virtual ~EventType() { }
    };
    deque<EventType*> event_queue;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    void queue_event(EventType*);

    /* event thread */
    bool stop;
    void event_entry();
    class EventThread: public Thread {
	Raft *owner;
    public:
	EventThread(Raft *o): owner(o) { }
	void *entry() {
	    owner->event_entry();
	    return NULL;
	}
    }event_thread;
    void stop_event_thread();

    /* time */
    class OnTimeOut: public Context {
	Raft *owner;
    public:
	OnTimeOut(Raft *o): owner(o) { }
	void finish();
    };

    friend class OnTimeOut;
    OnTimeOut *_onTimeOut;
    Timer timer;
    void reset_timeout(double sec = 2.0);

    /* RPC Server */
    void dispatch(string, int, void *);
    RPCServer *rpc_server;

    /* ... */
    virtual bool check_client_request(int len, void *req) = 0; 
    LogExecuter *log_executer;

    /* state machine event */
    struct EvRPC: sc::event<EvRPC>, EventType { 
	EvRPC(RPCRequestRef ref): 
	    EventType(RPC_t), req(ref) { };
	RPCRequestRef req;
    };

    struct EvVote: sc::event<EvVote>, EventType { 
	EvVote(RPCVoteRef ref): 
	    EventType(Vote_t), vote(ref) { }
	RPCVoteRef vote;
    };
    
    struct EvRPCRes: sc::event<EvRPCRes>, EventType {
	EvRPCRes(RPCResponseRef ref): 
	    EventType(Response_t), response(ref) { }
	RPCResponseRef response;
    };

    struct EvTimeOut: sc::event<EvTimeOut>, EventType { 
	EvTimeOut():
	    EventType(TimeOut_t) { }
    };
    
    struct EvClientReq: sc::event<EvClientReq>, EventType {
	EvClientReq(int l, shared_ptr<char> ref ) :
	    EventType(ClientReq_t), len(l), data(ref) { }
	int len;
	shared_ptr<char> data;
    };

    /* state machine */
    struct Active;
    struct Machine: sc::state_machine<Machine, Active> {
	Raft *owner;
	Machine(Raft *o): owner(o) { }
	Raft* get_owner() {
	    return owner;
	}
    };
    friend struct achine;

    /* sub State of state machine */
    struct Follower;
    struct Candidate;
    struct Leader;
    struct Active: sc::state<Active, Machine, Follower> {
	Active(my_context ctx) : my_base(ctx) { }
	/* we should move default reaction here ... */
    };
    struct Follower: sc::state< Follower, Active > {
	Follower(my_context);
	typedef boost::mpl::list <
	    sc::custom_reaction< EvRPC >,
	    sc::custom_reaction< EvVote >,
	    sc::transition<EvTimeOut, Candidate>
	    > reactions;
	sc::result react(const EvRPC&);
	sc::result react(const EvVote&);

    };
    struct Candidate: sc::state< Candidate, Active > {
	set<int> votedPeer;
	set<int> rejectedPeer;
	
	Candidate(my_context);
	typedef boost::mpl::list <
	    sc::custom_reaction< EvRPC >,
	    sc::custom_reaction< EvVote >, // maybe peer have higher term
	    sc::custom_reaction< EvRPCRes >,
	    sc::transition<EvTimeOut, Candidate>
	    > reactions;
	sc::result react(const EvRPC&);
	sc::result react(const EvVote&);
	sc::result react(const EvRPCRes&);
    };
    struct Leader: sc::state<Leader, Active> {
	struct Session {
	    int uuid;
	    int nextIndex;
	    int matchIndex;

	    static const int Detect = 0;
	    static const int Sync = 1;
	    int state;
	    
	    static const int max = 10;
	    set<int> inflight_RPC; // index of log entry
	    Session(int id, int next, int match) :
		uuid(id), nextIndex(next), matchIndex(match) {
		    state = Detect;
	    }
	};
	// Session only be used to track which Log entry should be sent to peer.
	map<int, Session> session_map;
	pthread_mutex_t lock;
	pthread_cond_t cond;

	void send_heartbeat(int id);
	RPCRequestRef build_request(Log::log_entry_t *);
	void push_log_entry(Session *);

	int seq;
	map<int, pair<Session*, int> > pending_RPC;
	bool stop;
	bool need_check;
	void notify_sender();
	void sender_entry();
	class Sender: public Thread {
	    Leader *owner;
	public:
	    Sender(Leader *l): 
		owner(l) { }
	    void *entry() {
		owner->sender_entry();
		return NULL;
	    }
	} sender;
	void stop_sender();

	Leader(my_context);
	~Leader();
	
	void update_commit_index();
	
	typedef boost::mpl::list <
	    sc::custom_reaction< EvClientReq >,
	    sc::custom_reaction< EvRPC >,
	    sc::custom_reaction< EvVote >,
	    sc::custom_reaction< EvRPCRes >,
	    sc::custom_reaction< EvTimeOut >
	    > reactions;
	sc::result react(const EvClientReq&);
	sc::result react(const EvRPC&);
	sc::result react(const EvVote&);
	sc::result react(const EvRPCRes&);
	sc::result react(const EvTimeOut&);
    };

    Machine state_machine;

public:
    Raft(int id, int term, int voted, Log *l, int commit, LogExecuter *le, RPCServer *s);
    virtual ~Raft();
};

#endif
