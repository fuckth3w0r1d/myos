#include "init.h"
#include "kernel/print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"

void init(void)
{
    print("init all start\n");
    _init_interrupt();      // 初始化中断
    _init_timer();          // 初始化时钟
    _init_mem();            // 初始化内存
    _init_thread();          // 初始化线程
    print("init all done\n");
}
