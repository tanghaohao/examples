#include "assert.h"
#include "raft.h"

void Raft::queue_event(EventType *e)
{
    pthread_mutex_lock(&lock);
    event_queue.push_back(e);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}

void Raft::event_entry()
{
    pthread_mutex_lock(&lock);
    while(!stop) {
	while(event_queue.empty() && !stop)
	    pthread_cond_wait(&cond, &lock);
	if (stop)
	    break;
	EventType *event = event_queue.front();
	event_queue.pop_front();
	pthread_mutex_unlock(&lock);
	switch(event->type) {
	    case EventType::TimeOut_t:
		{
		    EvTimeOut* e = (EvTimeOut*)event;
		    state_machine.process_event(*e);
		}
		break;
	    case EventType::RPC_t:
		{
		    EvRPC *e = (EvRPC*)event;
		    state_machine.process_event(*e);
		}
		break;
	    case EventType::Vote_t:
		{
		    EvVote *e = (EvVote*)event;
		    state_machine.process_event(*e);
		}
		break;
	    case EventType::Response_t:
		{
		    EvRPCRes *e = (EvRPCRes*)event;
		    state_machine.process_event(*e);
		}
		break;
	    case EventType::ClientReq_t:
		{
		    EvClientReq *e = (EvClientReq*)event;
		    state_machine.process_event(*e);
		}
		break;
	    default:
		std::cout << "unkown event(" << event->type << ")" << std::endl;
		assert(0);
	}
	delete event;
	pthread_mutex_lock(&lock);
    }
    pthread_mutex_unlock(&lock);
}

void Raft::dispatch(string method, int len, void *data)
{
    if(method.compare("RPC_Request")) {
	RPC_Request *req = (RPC_Request*)malloc(len);
	memcpy(req, data, len);
	EvRPC *e = new EvRPC(RPCRequestRef(req));
	queue_event(e);
    }
    else if (method.compare("RPC_Vote")) {
	RPC_Vote *vote = (RPC_Vote*)malloc(len);
	memcpy(vote, data, len);
	EvVote *e = new EvVote(RPCVoteRef(vote));
	queue_event(e);
    }
    else if(method.compare("RPC_Reponse")) {
	RPC_Response *res = (RPC_Response*)malloc(len);
	memcpy(res, data, len);
	EvRPCRes *e = new EvRPCRes(RPCResponseRef(res));
	queue_event(e);
    }
    else {
	assert(method.compare("Client_Req"));
	char *p = (char*)malloc(len);
	memcpy(p, data, len);
	EvClientReq *e = new EvClientReq(len, shared_ptr<char>(p));
	queue_event(e);
    }
}

void Raft::OnTimeOut::finish()
{
    owner->queue_event(new EvTimeOut());
}

void Raft::reset_timeout(double sec)
{
    if (_onTimeOut) {
	_onTimeOut = (OnTimeOut*)timer.cancel_event(_onTimeOut);
	if (_onTimeOut) 
	    delete _onTimeOut;
    }
    _onTimeOut = new OnTimeOut(this);
    timer.add_event_after(sec, _onTimeOut);

}

Raft::Follower::Follower(my_context ctx) :
    my_base(ctx)
{
    Raft *owner = context<Machine>().get_owner();
    owner->reset_timeout();
}

sc::result Raft::Follower::react(const EvRPC &event)
{
    const RPCRequestRef req = event.req;
    bool res = false;

    Raft *owner = context<Machine>().get_owner();
    if (req->term < owner->currentTerm)
	goto OUT;
    else if (req->term > owner->currentTerm) {
	owner->votedFor = -1;
	owner->currentTerm = req->term;
    }

    owner->reset_timeout();
    if (!req->len) 
	return discard_event();

    if (req->prevLogIndex) {
	Log::log_entry_ref ent = owner->log->get(req->prevLogIndex);
	if (ent == Log::log_entry_ref()
	    || ent->term != req->prevLogTerm)
	    goto OUT;
    }
    
    {
	int index = req->prevLogIndex + 1;
	const Log::log_entry_t *pent = (const Log::log_entry_t *)req->entry;
	Log::log_entry_ref ent = owner->log->get(index);
	if (ent != Log::log_entry_ref()
		&& ent->term != pent->term)
	    owner->log->clear_after(index-1);
	owner->log->append(*pent);
    }

    if (req->leaderCommit > owner->commitIndex) {
	owner->commitIndex = req->leaderCommit;
	owner->log_executer->update_last_committed(req->leaderCommit);
    }
    if (req->sender != owner->leader)
	owner->leader = req->sender;
    res = true;
OUT:
    /* send reply ... */
    RPC_Response reply;
    reply.seq = req->seq;
    reply.sender = owner->uuid;
    reply.type = RPC_Header::RESPONSE;
    reply.term = owner->currentTerm;
    reply.result = res;
    owner->rpc_server->call(req->sender, "RPC_Response", sizeof(reply), &reply);
    return discard_event();
}

sc::result Raft::Follower::react(const EvVote &event)
{
    RPCVoteRef vote = event.vote;
    bool voted = false;

    Raft *owner = context<Machine>().get_owner();
    if (vote->term < owner->currentTerm)
	goto Out;
    else if (vote->term > owner->currentTerm) {
	owner->currentTerm = vote->term;
	owner->votedFor = -1;
    }
    if (owner->votedFor == -1 
	    || owner->votedFor == vote->sender) {
	if (vote->lastLogTerm > owner->log->last_term())
	    voted = true;
	else if (vote->lastLogTerm == owner->log->last_term()
		&& vote->lastLogIndex >= owner->log->last_index())
	    voted = true;
    }
Out:
    if (voted)
	owner->votedFor = vote->sender;
    RPC_Response reply;
    reply.seq = vote->seq;
    reply.sender = owner->uuid;
    reply.type = RPC_Header::RESPONSE;
    reply.term = owner->currentTerm;
    reply.result = voted;
    owner->rpc_server->call(vote->sender, "RPC_Response", sizeof(reply), &reply);
    return discard_event();
}

Raft::Candidate::Candidate(my_context ctx)
    : my_base(ctx)
{
    set<int>::iterator it = peers.begin();
    Raft *owner = context<Machine>().get_owner();
    ++ owner->currentTerm;
    RPC_Vote vote;
    vote.seq = 0;
    vote.type = RPC_Header::VOTE;
    vote.sender = owner->uuid;
    vote.term = owner->currentTerm;
    vote.lastLogIndex = owner->log->last_index();
    vote.lastLogTerm = owner->log->last_term();
    while (it!=peers.end()) {
	if (*it != owner->uuid)
	    owner->rpc_server->call(*it, "RPC_Vote", sizeof(vote), &vote);
    }
    owner->reset_timeout();
}

sc::result Raft::Candidate::react(const EvRPC &event)
{
    RPCRequestRef req = event.req;
    Raft *owner = context<Machine>().get_owner();
    if (req->term >= owner->currentTerm) {
	post_event(event);
	return transit<Follower>();
    }
    else
	return discard_event();
}

sc::result Raft::Candidate::react(const EvVote &event)
{
    RPCVoteRef vote = event.vote;
    Raft *owner = context<Machine>().get_owner();
    if (vote->term > owner->currentTerm) {
	post_event(event);
	return transit<Follower>();
    }
    else 
	return discard_event();
}

sc::result Raft::Candidate::react(const EvRPCRes &event)
{
    RPCResponseRef res = event.response;
    int sender = res->sender;
    if (votedPeer.count(sender))
	return discard_event();
    if (res->result == true)
	votedPeer.insert(sender);
    else
	rejectedPeer.insert(sender);

    if (votedPeer.size() > peers.size()/2) {
	return transit<Leader>();
    }
    if (rejectedPeer.size() > peers.size()/2) {
	return transit<Follower>(); //
    }
    return discard_event();
}

Raft::Leader::Leader(my_context ctx)
    :my_base(ctx), sender(this)
{
    Raft *owner = context<Machine>().get_owner();
    int last = owner->log->last_index();
    int first = owner->log->first_index();
    for(set<int>::iterator it = peers.begin();
	    it != peers.end();
	    ++ it) {
	session_map.insert( pair<int, Session>(*it, Session(*it, last, first)));
    }
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    seq = 1;
    need_check = true;
    stop = false;
    sender.create();
    
    /* send heartbeat */
    for(set<int>::iterator it = peers.begin();
	    it != peers.end();
	    ++ it)
	send_heartbeat(*it);

    owner->reset_timeout(0.5);
}

Raft::Leader::~Leader()
{
    stop_sender();
}

void Raft::Leader::send_heartbeat(int peer)
{
    Raft *owner = context<Machine>().get_owner();
    RPC_Request hb;
    memset(&hb, 0, sizeof(hb));
    hb.seq = 0;
    hb.sender = owner->uuid;
    hb.type = RPC_Header::APPEND;
    hb.term = owner->currentTerm;
    owner->rpc_server->call(peer, "RPC_Append", sizeof(hb), &hb); 
}

RPCRequestRef Raft::Leader::build_request(Log::log_entry_t *e)
{
    Raft *owner = context<Machine>().get_owner();
    int datalen = sizeof(*e) + e->length;
    int size = datalen + sizeof(RPC_Request);
    RPC_Request *p = (RPC_Request*)malloc(size);
    memset(p, 0, size);
    p->seq = 0;
    p->type = RPC_Header::APPEND;
    p->term = owner->currentTerm;
    p->sender = owner->uuid;
    Log::log_entry_ref prev = owner->log->get(e->index - 1);
    if (prev != Log::log_entry_ref()) {
	p->prevLogIndex = prev->index;
	p->prevLogTerm = prev->term;
    }
    p->leaderCommit = owner->commitIndex;
    p->len = datalen;
    if (datalen) {
	memcpy(p->entry, e, datalen);
    }
    return RPCRequestRef(p);
}

void Raft::Leader::push_log_entry(Session *s)
{
    Raft *owner = context<Machine>().get_owner();
    if (s->state == Session::Detect && 
	    s->inflight_RPC.empty()) {
	pthread_mutex_lock(&lock);
	Log::log_entry_ref ent = owner->log->get(s->nextIndex);
	RPCRequestRef msg = build_request(&(*ent));
	msg->seq = seq++;
	s->inflight_RPC.insert(ent->index);
	pending_RPC[msg->seq] = make_pair<Session*, int> (s, msg->seq);

	owner->rpc_server->call(s->uuid, "RPC_Append",
		sizeof(RPC_Request) + msg->len, &(*msg));
    }
    else if(s->state == Session::Sync) {
	while (s->inflight_RPC.size() >= Session::max) {
	    int next = s->nextIndex;
	    Log::log_entry_ref ent = owner->log->get(next);
	    if (!ent) 
		return;
	    s->nextIndex ++;
	    RPCRequestRef msg = build_request(&(*ent));
	    msg->seq = seq ++;
	    s->inflight_RPC.insert(ent->index);
	    pending_RPC[msg->seq] = make_pair<Session*, int> (s, msg->seq);

	    owner->rpc_server->call(s->uuid, "RPC_Append", 
	    	sizeof(RPC_Request) + msg->len, &(*msg));
	}
    }
}

void Raft::Leader::sender_entry()
{
   while(!stop) {
       for(map<int, Session>::iterator it = session_map.begin();
	       it != session_map.end();
	       ++ it) {
	   push_log_entry(&(it->second));
       }

       pthread_mutex_lock(&lock);
       while(!need_check)
	   pthread_cond_wait(&cond, &lock);
       need_check = false;
       pthread_mutex_unlock(&lock);
   }
}

void Raft::Leader::notify_sender()
{
    pthread_mutex_lock(&lock);
    need_check = true;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
}

void Raft::Leader::stop_sender()
{
    if (stop)
	return;
    stop = true;
    notify_sender();
    sender.join();
}

sc::result Raft::Leader::react(const EvClientReq &event)
{
    Raft *owner = context<Machine>().get_owner();
    int len = event.len;
    shared_ptr<char> data = event.data;

    /* do what's you want before append it to log  */
    if (!owner->check_client_request(len, &(*data)))
	return discard_event();

    Log::log_entry_ref ent = owner->log->build_entry(len, &(*data));
    ent->term = owner->currentTerm;
    owner->log->append(*ent);
    notify_sender();
    return discard_event();
}

sc::result Raft::Leader::react(const EvRPC &event)
{
    Raft *owner = context<Machine>().get_owner();
    RPCRequestRef req = event.req;
    if (req->term > owner->currentTerm)
	return transit<Follower>(); // someone with higher term
    else if (req->term < owner->currentTerm) {
	send_heartbeat(req->sender); // send heartbeat to peer
    }
    else {
	assert(0 == "OMG, there are two leaders in the cluster!!");
    }
    return discard_event();
}

sc::result Raft::Leader::react(const EvVote &event)
{
    Raft *owner = context<Machine>().get_owner();
    RPCVoteRef vote = event.vote;
    if (vote->term > owner->currentTerm) {
	post_event(event); 
	return transit<Follower>(); // somebody with higher term want to be Leader
    }
    send_heartbeat(vote->sender);
    return discard_event();
}

void Raft::Leader::update_commit_index()
{
    multiset<int> match;
    for(map<int, Session>::iterator it = session_map.begin();
	    it != session_map.end();
	    ++ it) {
	match.insert(it->second.matchIndex);
    }
    int minority = match.size()/2;
    multiset<int>::iterator it = match.begin();
    if (minority) {
	while (--minority) it++;
    }
    Raft *owner = context<Machine>().get_owner();
    int index = *it; 
    Log::log_entry_ref ent = owner->log->get(index);
    if (ent != Log::log_entry_ref() 
	    && ent->term == owner->currentTerm){
	// leader only commit index under currentTerm
	owner->commitIndex = *it;
	/* now, we can do the client's requests */
	owner->log_executer->update_last_committed(*it);
    }
}

sc::result Raft::Leader::react(const EvRPCRes &event)
{
    Raft *owner = context<Machine>().get_owner();
    RPCResponseRef response = event.response;
    if (response->term > owner->currentTerm) {
	return transit<Follower>(); 
    }
    else if (response->term < owner->currentTerm) {
	send_heartbeat(response->sender);
	return discard_event();
    }
    int s = response->seq;
    if (pending_RPC.count(s) == 0) {
	std::cout << "no correspond RPC_Request exit" << std::endl;
	return discard_event();
    }

    Session *session = pending_RPC[s].first;
    int index = pending_RPC[s].second;
    pending_RPC.erase(s);
    if (response->result == false) {
	session->inflight_RPC.erase(index);
	session->nextIndex --;
	if (session->state != Session::Detect) {
	    std::cout << "follower will never reject RPCRequest in Sync state" << std::endl;
	    assert(0);
	}
	notify_sender();
	return discard_event();
    }
    
    if (session->state == Session::Detect)
	session->state == Session::Sync;

    session->inflight_RPC.erase(index);
    set<int>::iterator it = session->inflight_RPC.begin();
    if (it == session->inflight_RPC.end())
	session->matchIndex = session->nextIndex - 1;
    else 
	session->matchIndex = *it - 1;
    
    update_commit_index();
    notify_sender();
    return discard_event();
}

sc::result Raft::Leader::react(const EvTimeOut &evnet)
{
    Raft *owner = context<Machine>().get_owner(); 
    for(set<int>::iterator it = peers.begin();
	    it != peers.end();
	    ++ it)
	if (*it != owner->uuid)
	    send_heartbeat(*it);
    owner->reset_timeout();
    return discard_event();
}

Raft::Raft(int id, int term, int voted, Log *l, int commit, LogExecuter *le, RPCServer *s) :
    uuid(id),
    currentTerm(term),
    votedFor(voted),
    log(l),
    commitIndex(commit),
    event_thread(this),
    log_executer(le),
    rpc_server(s),
    state_machine(this) 
{
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    stop = false;
    event_thread.create();
    timer.start_tick();
    log_executer->start();
    state_machine.initiate();
}

Raft::~Raft()
{
    timer.stop_tick();
    stop_event_thread();
    log_executer->stop();
}
