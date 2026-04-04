#ifndef __DEVICE_CONSOLE_H
#define __DEVICE_CONSOLE_H
#include "stdint.h"
void _init_console(void);
void console_acquire(void);
void console_release(void);
void console_put_str(char* str);
void console_put_char(uint8_t char_asci);
void console_put_int(int32_t num);
void console_put_uint(uint32_t num);
// 用宏来封装这四个线程安全的终端打印函数
#define console_put(x) _Generic((x), \
    char: console_put_char, \
    unsigned char: console_put_char, \
    char*: console_put_str, \
    const char*: console_put_str, \
    int: console_put_int, \
    unsigned int: console_put_uint, \
    default: console_put_str \
)(x)
#endif
