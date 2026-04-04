#include "init.h"
#include "debug.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "kernel/print.h"
#include "keyboard.h"
#include "kernel/ioqueue.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*);

int main(void)
{
   print("kernel made by r3t2\n");
   init();
   char* strA = " thread_A_";
   char* strB = " thread_B_";
   thread_create("testA", 31, k_thread_a, strA);
   thread_create("testB", 31, k_thread_b, strB);
   _enable_intr();
   while(1);
   // {
   //    console_put("main thread");
   // }
   return 0;
}

void k_thread_a(void* arg)
{
   while(1)
   {
      _intr_status old_status = _disable_intr();
      if(!ioq_empty(&kbd_buf))
      {
         console_put(arg);
         char byte = ioq_getchar(&kbd_buf);
         console_put(byte);
      }
      _set_intr_status(old_status);
   }
}
void k_thread_b(void* arg)
{
   while(1)
   {
      _intr_status old_status = _disable_intr();
      if(!ioq_empty(&kbd_buf))
      {
         console_put(arg);
         char byte = ioq_getchar(&kbd_buf);
         console_put(byte);
      }
      _set_intr_status(old_status);
   }
}
