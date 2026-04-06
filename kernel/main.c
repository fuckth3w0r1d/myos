#include "init.h"
#include "debug.h"
#include "stdtype.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "kernel/print.h"
#include "keyboard.h"
#include "kernel/ioqueue.h"
#include "process.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
volatile int test_var_a = -1, test_var_b = -1;

int main(void)
{
   print("kernel made by r3t2\n");
   init();
   // char* strA = " thread_A_";
   // char* strB = " thread_B_";
   thread_create("testA", 31, k_thread_a, NULL);
   thread_create("testB", 31, k_thread_b, NULL);
   process_execute(u_prog_a, "user_prog_a");
   process_execute(u_prog_b, "user_prog_b");
   _enable_intr();
   while(1);
   // {
   //    console_put("main thread");
   // }
   return 0;
}

void k_thread_a(void* arg)
{
   //char* para = arg;
   while(1)
   {
      console_put("v_a:");
      console_put(test_var_a);
      console_put("   ");
   }
}
void k_thread_b(void* arg)
{
   //char* para = arg;
   while(1)
   {
      console_put("v_b:");
      console_put(test_var_b);
      console_put("   ");
   }
}

void u_prog_a(void)
{
   while(1)
   {
      test_var_a++;
   }
}

void u_prog_b(void)
{
   while(1)
   {
      test_var_b++;
   }
}
