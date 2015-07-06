#include <stdio.h>
#include "tp.h"

int main()
{
    int count = 100;
    while(count--) {
	tracepoint(th, tracepoint_1, count, "aaa");
    }
    return 0;
}
