#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER th

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "./tp.h"

#if !defined(_TP_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _TP_H

#include <lttng/tracepoint.h>

TRACEPOINT_EVENT(
	th,
	tracepoint_1,
	TP_ARGS(
	    int, key,
	    char*, value
	),
	TP_FIELDS(
	    ctf_integer(int, key, key)
	    ctf_string(value, value)
	)
    )

#endif

#include <lttng/tracepoint-event.h>
