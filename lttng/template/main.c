#include <stdio.h>
#include "tp.h"

int main()
{
    int count = 0;
    while(count < 10) {
	tracepoint(osd, do_osd_op_pre_copy_get, "XXXX", count);
	count ++;
    }
}
