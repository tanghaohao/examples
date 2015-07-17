#ifndef LOG_H
#define LOG_H

#include <stdlib.h>
#include <string.h>
#include <tr1/memory>
#include <stdint.h>
#include <deque>
#include <map>
#include "assert.h"

using std::deque;
using std::map;
using std::tr1::shared_ptr;

class Log {
    public:
	struct log_entry_t {
	    uint32_t index;
	    uint32_t term;
	    uint32_t length;
	    uint8_t data[0];
	};
	typedef shared_ptr<log_entry_t> log_entry_ref;

	log_entry_ref build_entry(int len, void *data) {
	    int size = sizeof(log_entry_t) + len;
	    log_entry_t *p = (log_entry_t*)malloc(size);
	    p->index = nextIndex;
	    p->term = 0;
	    p->length = len;
	    memcpy(p->data, data, len);
	    return log_entry_ref(p);
	}

	log_entry_ref clone(const log_entry_t *e) {
	    int size = sizeof(*e) + e->length;
	    log_entry_t *p = (log_entry_t*)malloc(size);
	    p->index = e->index;
	    p->term = e->term;
	    p->length = e->length;
	    memcpy(p->data, e->data, e->length);
	    log_entry_ref pent(p);
	    return pent;
	};

	int append(const log_entry_t &e) {
	    log[e.term].push_back(clone(&e));
	    return nextIndex++;
	}

	log_entry_ref get(uint32_t index) {
	    if(index >= nextIndex ||
		    index < firstIndex)
		return log_entry_ref();
	    uint32_t distance = index - firstIndex;
	    map<uint32_t, deque<log_entry_ref> >::iterator it = log.begin();
	    while(it != log.end()) {
		if (distance < it->second.size()) {
		    return it->second[distance];
		}
		distance -= it->second.size();
		++ it;
	    }
	    assert( 0 == "should not be here !");
	    return log_entry_ref();
	}

	/* erase log entry whose _index > index */
	int clear_after(uint32_t index) {
	    if (index >= nextIndex)
		return 0;
	    map<uint32_t, deque<log_entry_ref> >::reverse_iterator it = log.rbegin();
	    uint32_t first = nextIndex;
	    while(it != log.rend()) {
		first -= it->second.size();
		if (index < first) 
		    it->second.clear();
		else {
		    int num = it->second.size() - (index-first);
		    assert(num > 0);
		    while(--num) 
			it->second.pop_back();
		}
		-- it;
	    }
	    nextIndex = index+1;
	    return 0;
	}

	int trim(uint32_t index) {
	    if (index < firstIndex) 
		return 0;
	    map<uint32_t, deque<log_entry_ref> >::iterator it = log.begin();
	    while(it != log.end()) {
		if (firstIndex + it->second.size() < index) {
		    firstIndex += it->second.size();
		    log.erase(it++);
		}
		else {
		    while(firstIndex < index) {
			it->second.pop_front();
			firstIndex += 1;
		    }
		    break;
		}
	    }
	    if (index > firstIndex)
		firstIndex = index;
	    if (index  > nextIndex)
		nextIndex = index+1;
	    return 0;

	}

	uint32_t last_index() { return nextIndex - 1; }
	uint32_t last_term() {
	    map<uint32_t, deque<log_entry_ref> >::reverse_iterator it = log.rbegin();
	    if (it!=log.rend())
		return it->first;
	    return -1;
	}
	uint32_t first_index() { return firstIndex; }
	uint32_t first_term() { 
	    map<uint32_t, deque<log_entry_ref> >::iterator it = log.begin();
	    if (it!=log.end())
		return it->first;
	    return -1;
	}

	Log() :
	    firstIndex(0),
	    nextIndex(0) { }
    private:
	map<uint32_t, deque<log_entry_ref> > log; // term -> log
	uint32_t firstIndex;
	uint32_t nextIndex;
};

#endif
