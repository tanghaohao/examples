#lttng的例子

## 静态库的方式
tp.h 文件定义了tracepoint provider，以及相应的tracepoint <br>
tp.c 在定义了宏之后，再引用tp.h，从而产生trace所需要的函数和全局结构体 <br>
main.c 引用tp.h，由于没有定义相应的宏，所以它看到的只是申明 <br>
下面对应的编译指令: <br>
```shell
gcc -c -I. tp.c
ar rc tp.o // 打包成静态库
gcc -c main.c
gcc -o app main.o tp.a -llttng-ust -ldl // 需要注意main.o必须在tp.a的前面
./app
```

## 动态链接库的方式
在代码上，tp.c和tp.h和前面的一样，但是main函数稍微有点差别，必须在引用tp.h之前定义如下两个宏:<br>
```cpp
#define TRACEPOINT_DEFINE
#define TRACEPOINT_PROBE_DYNAMIC_LINKAGE
#include "tp.h"
```
TRACEPOINT_PROBE_DYNAMIC_LINKAGE宏帮助tp.h中使用的宏意识到，它们将以动态链接库的形式加载到主程序中。<br>
下面是相应的编译指令:<br>
```shell
gcc -fpic -I. tp.c
gcc -shared -Wl,--no-as-needed -o tp.so -llttng-ust tp.o
gcc -c main.c
gcc -o app main.o -ldl
LD_PRELOAD=tp.so ./app
```
