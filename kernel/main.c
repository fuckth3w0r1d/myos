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
#include "user/syscall.h"
#include "syscall-init.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

volatile int prog_a_pid = 0, prog_b_pid = 0;

int main(void)
{
   print("kernel made by r3t2\n");
   init();
   // char* strA = " thread_A_";
   // char* strB = " thread_B_";
   process_execute(u_prog_a, "user_prog_a");
   process_execute(u_prog_b, "user_prog_b");
   thread_create("testA", 31, k_thread_a, NULL);
   thread_create("testB", 31, k_thread_b, NULL);
   _enable_intr();
   while(1);
   // {
   //    console_put("main thread");
   // }
   return 0;
}

void k_thread_a(void* arg)
{
   (void)arg;
   while(1)
   {
      console_put("p_a:");
      console_put(prog_a_pid);
      console_put("p_b:");
      console_put(prog_b_pid);
      console_put("\n");
   }
}
void k_thread_b(void* arg)
{
   (void)arg;
   while(1)
   {
      console_put("p_a:");
      console_put(prog_a_pid);
      console_put("p_b:");
      console_put(prog_b_pid);
      console_put("\n");
   }
}

void u_prog_a(void)
{
   while(1)
   {
      prog_a_pid = getpid();
   }
}

void u_prog_b(void)
{
   while(1)
   {
      prog_b_pid = getpid();
   }
}
