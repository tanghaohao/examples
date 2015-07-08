#include <iostream>
#include "assert.h"

class Test {
private:
    void fun1(int deep) {
	assert(deep);
	if (deep > 0)
	    fun1(deep-1);
    }
    void fun2(int deep) {
	assertf(deep, "deep = %d", deep);
	if (deep > 0)
	    fun2(deep-1);
    }
public:
    void runTest1(int loops) {
	fun1(loops);
    }
    void runTest2(int loops) {
	fun2(loops);
    }

};

int main()
{
    Test t;
    t.runTest2(4);
}
