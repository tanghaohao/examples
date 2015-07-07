AUTOMAKE_OPTIONS = foreign
LTTNG_GEN_TP=lttng-gen-tp
%.c %.h: %.tp
	$(LTTNG_GEN_TP) $< -o $*.c -o $*.h

nodist_libmytrace_tp_la_SOURCES = tp.c
libmytrace_tp_la_LIBADD = -llttng-ust -ldl
libmytrace_tp_la_CPPFLAGS = -I. -DTRACEPOINT_PROBE_DYNAMIC_LINKAGE
noinst_LTLIBRARIES = libmytrace_tp.la
