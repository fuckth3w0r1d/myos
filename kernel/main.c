#include "kernel/print.h"
#include "init.h"
#include "debug.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"

void tA(void*);
void tB(void*);

int main(void)
{
   print("kernel made by r3t2\n");
   init();
   char* strA = "TA";
   char* strB = "TB";
   thread_create("testA", 31, tA, strA);
   thread_create("testB", 8, tB, strB);
   _enable_intr();
   while(1)
   {
      print("main thread");
   }
   return 0;
}

void tA(void* arg)
{
   char* str = arg;
   while(1)
   {
      print(str);
   }
}

void tB(void* arg)
{
   char* str = arg;
   while(1)
   {
      print(str);
   }
}
