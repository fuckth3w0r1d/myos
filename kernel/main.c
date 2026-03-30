#include "kernel/print.h"
#include "init.h"
#include "debug.h"
#include "interrupt.h"
#include "memory.h"
int main(void)
{
   print("kernel made by r3t2\n");
   init();
   void* addr = alloc_kernel_pages(3);
   print("get kernel pages start vaddr is ");
   print((uint32_t)addr);
   while(1);
   return 0;
}
