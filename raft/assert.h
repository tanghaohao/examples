#ifndef _ASSERT_H
#define _ASSERT_H

#if defined(__linux__)
#include <features.h>
#elif defined(__FreeBSD__)
#include<sys/cdefs.h>
#define __GNUC_PREREQ(min, maj) __GNUC_PREREQ__(min, maj)
#endif

#include "backTrace.h"

struct FailedAssertion {
    BackTrace *backtrace;
    FailedAssertion(BackTrace *bt) 
	:backtrace(bt) { }
};

#ifdef __GNUC_PREREQ

#if defined __cplusplus ? __GNUC_PREREQ (2, 6) : __GNUC_PREREQ (2, 4) 
#define HAVE_PRETTY_FUNCTION
#endif

#endif

#if defined(HAVE_PRETTY_FUNCTION)
#define __ASSERT_FUNCTION__ __PRETTY_FUNCTION__
#else
#define __ASSERT_FUNCTION__ __func__
#endif

void __assert_fail(const char *exp, const char *file, int line, const char *function)
    __attribute__((__noreturn__));
void __assert_fail(const char *exp, const char *file, int line, const char *function, const char *msg, ...)
    __attribute__((__noreturn__));

#define assert(exp)	\
	((exp)		\
	 ? (void)(0)	\
	 : __assert_fail(__STRING(exp), __FILE__, __LINE__, __ASSERT_FUNCTION__))

#define assertf(exp, ...)    \
	((exp)		    \
	 ? (void)(0)	    \
	 : __assert_fail(__STRING(exp), __FILE__, __LINE__, __ASSERT_FUNCTION__, __VA_ARGS__)) 

#endif
