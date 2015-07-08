#ifndef _BACKTRACE_H
#define _BACKTRACE_H

#include <iosfwd>
#include <execinfo.h>
#include <stdlib.h>
#include <string.h>

struct BackTrace {
    const static int max = 100;
    void *stack[max];
    ssize_t size;
    char **strings;
    int skip;

    BackTrace(int s = 0) {
	skip = s;
	size = backtrace(stack, max);
	strings = backtrace_symbols(stack, size);
    }
    BackTrace(const BackTrace& other) {
	strings = (char**)malloc(other.size);
	size = other.size;
	skip = other.skip;
	if (strings) {
	    memcpy(strings, other.strings, size);
	}
    }
    ~BackTrace() {
	free(strings);
    }
    void print(std::ostream& out);
};

#endif
