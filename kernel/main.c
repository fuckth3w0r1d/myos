#include "print.h"
#include "init.h"
#include "debug.h"
int main(void)
{
   print("kernel made by r3t2\n");
   init();
   ASSERT(1 == 2);
   while(1);
   return 0;
}
