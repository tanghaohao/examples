#include <ostream>
#include <cxxabi.h>
#include <stdlib.h>
#include <string.h>

#include "BackTrace.h"

void BackTrace::print(std::ostream& out)
{
    for (int i=skip; i<size; i++) {
	size_t size = 1024;
	char *function = (char*)malloc(size);
	if (!function)
	    return;

	char *start = NULL, *end = NULL;
	for(char *p = strings[i]; *p; ++p) 
	    if (*p == '(')
		start = p+1;
	    else if (*p == '+')
		end = p;
	if (start && end) {
	    int len = end - start;
	    char *foo = (char*)malloc(len);
	    if(!foo) {
		free(function);
		return;
	    }
	    memcpy(foo, start, len);
	    foo[len] = 0;
	    int status;
	    char *ret = abi::__cxa_demangle(foo, function, &size, &status);
	    if (ret)
		function = ret;
	    else {
		strncpy(function, foo, size);
		strncat(function, "()", size);
		function[size-1] = 0;
	    }
	    out << " " << function << std::endl;
	    free(foo);
	}
	else {
	    out << " " << strings[i] << std::endl;
	}
	free(function);	    
    }
}
