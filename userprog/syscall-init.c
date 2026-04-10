#include "syscall-init.h"
#include "kernel/print.h"
#include "user/syscall.h"
#include "thread.h"
#include "console.h"
#include "string.h"
#include "memory.h"
#define syscall_nr 32
typedef void* syscall;
syscall syscall_table[syscall_nr];

// 返回当前任务的pid
uint32_t sys_getpid(void)
{
    return running_thread()->pid;
}

// 打印字符串（未实现文件系统版本）
uint32_t sys_write(char* str)
{
    console_put(str);
    return strlen(str);
}
// 初始化系统调用
void _init_syscall(void)
{
    print("syscall_init_start\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE] = sys_write;
    print("_init_syscall done\n");
}
