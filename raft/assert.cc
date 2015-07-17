#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdarg.h>
#include "backTrace.h"
#include "assert.h"

using std::ostringstream;

void __assert_fail(const char *exp, const char *file, int line, const char *function)
{
    BackTrace *bt = new BackTrace(2);

    ostringstream oss;
    oss << file << "[" << line << "]: in function '" << function << "' pthread_id = " << pthread_self() \
	<< ": FAILED assert(" << exp << ")" << std::endl;
    oss << "backtrace detail:" << std::endl;
    bt->print(oss);
    std::cerr << oss.str();
    throw FailedAssertion(bt);
}

void __assert_fail(const char *exp, const char *file, int line, const char *function, const char *msg, ...)
{
    class Appender {
    public:
	Appender(char *buf, int len) :
	    m_buf(buf), m_len(len) { }

	void printf(const char *format, ...) {
	    va_list args;
	    va_start(args, format);
	    this->vprintf(format, args);
	    va_end(args);
	}
	void vprintf(const char *format, va_list args) {
	    int n = vsnprintf(m_buf, m_len, format, args);
	    if (n >= 0) {
		if (n < m_len) {
		    m_len -= n;
		    m_buf += n;
		}
		else {
		    m_len = 0;
		}
	    }
	}
    private:
	char *m_buf;
	int m_len;
    };

    char buf[4096];
    Appender tmp(buf, sizeof(buf));
    BackTrace *bt = new BackTrace(2);
    tmp.printf("%s[%d]: in function'%s' pthread_id = %llx\n", \
	    file, line, function, (unsigned long long)pthread_self());
    tmp.printf("Failed Assert(%s): ", exp);
    va_list args;
    va_start(args, msg);
    tmp.vprintf(msg, args);
    va_end(args);
    std::cerr << buf << std::endl;

    ostringstream oss;
    oss << "backtrace detail:" << std::endl;
    bt->print(oss);
    std::cerr << oss.str() << std::endl;
    throw FailedAssertion(bt);
}
