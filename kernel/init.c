#include "init.h"
#include "print.h"
#include "interrupt.h"

void initAll(void)
{
  put_str("init All\n");
  initInterrupt();      //初始化中断
}
