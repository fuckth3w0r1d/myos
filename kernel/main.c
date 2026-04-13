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
#include "stdio.h"

void k_thread_a(void*);     //自定义线程函数
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

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
   //void* p = alloc_kernel_pages(3);
   void* p1 = sys_malloc(0x100);
   void* p2 = sys_malloc(0x101);
   void* p3 = sys_malloc(0x450);
   printf(" thread_a malloc addr:0x%x,0x%x,0x%x\n", (int)p1, (int)p2, (int)p3);
   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   sys_free(p1);
   sys_free(p2);
   sys_free(p3);
   while(1);
}
void k_thread_b(void* arg)
{
   (void)arg;
   //void* p = alloc_kernel_pages(3);
   void* p1 = sys_malloc(0x100);
   void* p2 = sys_malloc(0x101);
   void* p3 = sys_malloc(0x450);
   printf(" thread_b malloc addr:0x%x,0x%x,0x%x\n", (int)p1, (int)p2, (int)p3);
   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   sys_free(p1);
   sys_free(p2);
   sys_free(p3);
   while(1);
}

void u_prog_a(void)
{
   //while(1);
   void* p1 = malloc(0x100);
   void* p2 = malloc(0x101);
   void* p3 = malloc(0x450);
   printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)p1, (int)p2, (int)p3);
   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   free(p1);
   free(p2);
   free(p3);
   while(1);
}

void u_prog_b(void)
{
   //while(1);
   void* p1 = malloc(0x100);
   void* p2 = malloc(0x101);
   void* p3 = malloc(0x450);
   printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)p1, (int)p2, (int)p3);
   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   free(p1);
   free(p2);
   free(p3);
   while(1);
}
