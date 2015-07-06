#include <stdio.h>
#define TRACEPOINT_DEFINE
#define TRACEPOINT_PROBE_DYNAMIC_LINKAGE
#include "tp.h"

int main()
{
    int count = 10;
    while(count--) {
	printf("%d\n", count);
	tracepoint(th, tracepoint_1, count, "aaa");
    }
    return 0;
}
