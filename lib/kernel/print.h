#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H
#include "stdint.h"

void _put_char(uint8_t char_asci);
void _put_str(char* message);
void _put_uint(uint32_t num);     //以十六进制打印
void _put_int(int32_t num);
// print宏来封装这四个打印函数
#define print(x) _Generic((x), \
    char: _put_char, \
    unsigned char: _put_char, \
    char*: _put_str, \
    const char*: _put_str, \
    int: _put_int, \
    unsigned int: _put_uint, \
    default: _put_str \
)(x)

#endif
