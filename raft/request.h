#ifndef REQUEST_H
#define REQUEST_H

#include <stdio.h>
#include <stdint.h>
#include <tr1/memory>

using std::tr1::shared_ptr;

/* RPC Request */
struct RPC_Header {
    int seq;
    enum { APPEND, VOTE, RESPONSE } type;
    int sender;
    int term;
};

struct RPC_Request: RPC_Header {
    int prevLogIndex;	// index of log entry immediately preceding new ones
    int prevLogTerm;	// term of the prevLogIndex entry
    int leaderCommit;	// leader's last commintIndex
    int len;
    char entry[0];
};

struct RPC_Vote : RPC_Header {
    int lastLogIndex;	// index of candidate's last log entry
    int lastLogTerm;	// term of candidate's last log entry'
};

struct RPC_Response: RPC_Header {
    bool result;
}; 

typedef shared_ptr<RPC_Request> RPCRequestRef;
typedef shared_ptr<RPC_Vote> RPCVoteRef;
typedef shared_ptr<RPC_Response> RPCResponseRef;

#endif
