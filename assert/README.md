#Assert and BackTrace
这里实现了两组宏：<br>
```cpp
assert(expr) // example: assert (ret == 0)
assert(expr, ...) // example: assert (ret, "return value = ", ret) 
```
当assert检查的表达式为false时，就会借助backtrace当前函数的调用栈<br>
test.cc文件是使用assert的一个例子。
#使用
使用automake来编译代码时默认使用-O2优化标志，此时得到的执行程序关闭栈帧，因而无法借助backtrace打印出当前的调用路径。为此在执行make时应该加上CXXFLAGS = '-rdynamic'.<br>
```shell
  ./autorun
  ./configure
  make CXXFALGS='-rdynamic'
```
