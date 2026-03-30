#include "init.h"
#include "kernel/print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"

void init(void)
{
    print("init All\n");
    _init_interrupt();      // 初始化中断
    _init_timer();          // 初始化时钟
    _init_mem();            // 初始化内存
}
