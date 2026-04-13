#ifndef __THREAD_H
#define __THREAD_H
#include "stdint.h"
#include "kernel/list.h"
#include "memory.h"
#define MAX_FILES_OPEN_PER_PROC 8
#define TASK_NAME_LEN 16
typedef void thread_func(void*); // 自定义通用函数类型，它将在很多线程函数中作为形参类型
typedef int16_t pid_t;   //进程的pid类型

// 进程或线程的状态
enum _task_status{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

// 中断栈
// 进程或线程被外部中断或软中断打断时，会按照此结构压入上下文
// 此栈在线程自己的内核栈中位置固定，所在页的最顶端
struct _intr_stack{
    uint32_t vec;     // 中断向量号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;       //虽然pushad把esp也压入栈中，但esp是不断变化的，所以popad会丢弃栈中的esp不会恢复
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    // 以下由cpu从低特权级进入高特权级时压入
    uint32_t err_code;        // err_code会被压入eip之后
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

// 线程栈 用于存储线程中待执行的函数
//  此结构在线程自己的内核栈中实际位置取决于实际运行情况
//  仅用在_switch_task时保存线程环境
struct _thread_stack{
    // 根据 ABI 以下寄存器应该保存
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    // 线程第一次执行的时候，eip指向待调用的函数 kernel_thread 其他时候，eip指向 switch_task 的返回地址
    void (*eip)(thread_func* func, void* func_arg);
    //以下仅供第一次被调度上CPU时使用
    void (*unused_retaddr);       // 占位
    thread_func* function;        // 由kernel_thread所调用的函数
    void* func_arg;               // kernel_thread所调用的函数所需要的参数
};

// 进程或线程的控制块
struct _task_struct{
    uint32_t* self_kstack;        //各内核线程都用自己的内核栈，这个成员指向栈顶
    pid_t pid;
    enum _task_status status;
    char name[16];
    uint8_t priority;             //线程优先级
    uint8_t ticks;                //每次在处理器上执行的tick数
    uint32_t elapsed_ticks;       // 此任务自从上cpu运行后至今占用了多少tick数
    struct list_elem general_tag; // general_tag的作用是用于线程在一般的队列中的结点
    struct list_elem all_list_tag; // all_list_tag的作用是用于线程队列thread_all_list中的节点
    uint32_t* pgdir;              //进程自己页目录表的虚拟地址
    struct _vm_pool user_vm;       // 用户进程的独立虚拟内存
    struct _chunk_desc user_chunkdescs[DESC_CNT];
    uint32_t canary;         //栈的边界标记，用于检测栈的溢出
};

extern struct list thread_ready_list;
extern struct list thread_all_list;

struct _task_struct* running_thread(void);
void thread_init_stack(struct _task_struct* pthread, thread_func function, void* func_arg);
void thread_init_info(struct _task_struct* pthread, char* name, uint8_t prio);
struct _task_struct* thread_create(char* name, uint8_t prio, thread_func function, void* func_arg);
void task_schedule(void);
void thread_block(enum _task_status stat);
void thread_unblock(struct _task_struct* pthread);
void _init_thread(void);
#endif
