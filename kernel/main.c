#include "init.h"
#include "debug.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "kernel/print.h"

void tA(void*);
void tB(void*);

int main(void)
{
   print("kernel made by r3t2\n");
   init();
   char* strA = "thread A";
   char* strB = "thread B";
   thread_create("testA", 31, tA, strA);
   thread_create("testB", 8, tB, strB);
   _enable_intr();
   while(1)
   {
      console_put("main thread");
   }
   return 0;
}

void tA(void* arg)
{
   char* str = arg;
   while(1)
   {
      console_put(str);
   }
}

void tB(void* arg)
{
   char* str = arg;
   while(1)
   {
      console_put(str);
   }
}
