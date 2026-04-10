#include "console.h"
#include "kernel/print.h"
#include "stdint.h"
#include "sync.h"
#include "thread.h"

static struct lock console_lock;    //控制台锁

// 初始化终端
void _init_console(void)
{
    lock_init(&console_lock);
}

// 获取终端
void console_acquire(void)
{
    lock_acquire(&console_lock);
}

// 释放终端
void console_release(void)
{
    lock_release(&console_lock);
}

// 终端输出字符串
void console_put_str(char* str)
{
    console_acquire();
    _put_str(str);
    console_release();
}

// 终端输出字符
void console_put_char(uint8_t char_asci)
{
    console_acquire();
    _put_char(char_asci);
    console_release();
}

// 终端输出无符号十六进制整数
void console_put_uint(uint32_t num)
{
    console_acquire();
    _put_uint(num);
    console_release();
}

// 终端输出有符号十六进制整数
void console_put_int(int32_t num)
{
    console_acquire();
    _put_int(num);
    console_release();
}
