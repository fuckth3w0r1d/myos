#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"

void init(void)
{
    print("init All\n");
    _init_interrupt();      // 初始化中断
    _init_timer();          // 初始化时钟
}
