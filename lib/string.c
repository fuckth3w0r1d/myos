#include "string.h"
#include "global.h"
#include "debug.h"
#include "stdtype.h"

char* strcpy(char* dst_, const char* src_)
{
    ASSERT(dst_ != NULL && src_ != NULL);
    char* dst = dst_;
    while((*dst_++ = *src_++));    //赋值语句，这里若赋值为\x00则会跳出循环
    return dst;
}

uint32_t strlen(const char* str)
{
    ASSERT(str != NULL);
    const char* p = str;
    while(*p++);
    return (p - str - 1);
}

int8_t strcmp(const char* a, const char* b)
{
    ASSERT(a != NULL && b != NULL);
    while(*a != 0 && *a == *b)
    {
        a++;
        b++;
    }
    return *a < *b ? -1 : *a > *b ;       //这里如果*a > *b 则返回1, 若相等则说明表达式不成立返回0,表示相等
}

char* strchr(const char* str, const uint8_t ch)
{
    ASSERT(str != NULL);
    while(*str != 0)
    {
        if(*str == ch) return (char*)str;
        str++;
    }
    return NULL;
}

char* strrchr(const char* str, const uint8_t ch)
{
    ASSERT(str != NULL);
    const char* last_char = NULL;
    while(*str != 0)
    {
        if(*str == ch) last_char = str;
        str++ ;
    }
    return (char*)last_char;
}

char* strcat(char* dst_, const char* src_)
{
    ASSERT(dst_ != NULL && src_ != NULL);
    char* str = dst_;
    while(*str++);
    str--;            //这里是因为str指针目前指向\x00，所以我们需要将指针减一
    while((*str++ = *src_++));
    return dst_;
}

uint32_t strchrs(const char* str, uint8_t ch)
{
    ASSERT(str != NULL);
    uint32_t ch_cnt = 0;
    const char* p = str;
    while(*p != 0)
    {
        if(*p == ch) ch_cnt++;
        p++;
    }
    return ch_cnt;
}
