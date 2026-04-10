#include "syscall.h"

// 无参数的系统调用
#define _syscall0(NUMBER) ({    \
    int retval;                 \
    asm volatile(               \
        "int $0x80"              \
        : "=a"(retval)          \
        : "a"(NUMBER)           \
        : "memory"              \
        );                      \
    retval;                     \
    })

// 一个参数的系统调用
#define _syscall1(NUMBER, ARG_1) ({    \
    int retval;                 \
    asm volatile(               \
        "int $0x80"              \
        : "=a"(retval)          \
        : "a"(NUMBER), "b"(ARG_1)           \
        : "memory"              \
        );                      \
    retval;                     \
    })

// 两个参数的系统调用
#define _syscall2(NUMBER, ARG_1, ARG_2) ({    \
    int retval;                 \
    asm volatile(               \
        "int $0x80"              \
        : "=a"(retval)          \
        : "a"(NUMBER), "b"(ARG_1), "c"(ARG_2)           \
        : "memory"              \
        );                      \
    retval;                     \
    })

// 三个参数的系统调用
#define _syscall3(NUMBER, ARG_1, ARG_2, ARG_3) ({    \
    int retval;                 \
    asm volatile(               \
        "int $0x80"              \
        : "=a"(retval)          \
        : "a"(NUMBER), "b"(ARG_1), "c"(ARG_2), "d"(ARG_3)           \
        : "memory"              \
        );                      \
    retval;                     \
    })

// 系统调用getpid
uint32_t getpid()
{
    return _syscall0(SYS_GETPID);
}

// 系统调用write
uint32_t write(char* str)
{
    return _syscall1(SYS_WRITE, str);
}
