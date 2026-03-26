#include "print.h"
#include "init.h"
int main(void)
{
   put_str("kernel made by r3t2\n");
   initAll();
   asm volatile("sti");
   while(1);
   return 0;
}
