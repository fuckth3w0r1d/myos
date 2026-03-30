#include "debug.h"
#include "memfunc.h"
#include "global.h"
#include "stdtype.h"

void memset(void* dst, uint8_t val, uint32_t size)
{
    ASSERT(dst != NULL);
    uint8_t* p = (uint8_t*)dst;
    while(size > 0)
    {
        *p++ = val;
        size--;
    }
}

void memcpy(void* dst, const void* src, uint32_t size)
{
    ASSERT(dst != NULL && src != NULL);
    uint8_t* d = (uint8_t*) dst;
    uint8_t* s = (uint8_t*) src;
    while(size > 0)
    {
        *d++ = *s++;
        size--;
    }
}

int memcmp(const void* a_, const void* b_, uint32_t size)
{
    const char* a = a_;
    const char* b = b_;
    ASSERT(a != NULL && b != NULL);
    while(size > 0)
    {
        if(*a != *b) return *a > *b ? 1 : -1;
        a++;
        b++;
        size--;
    }
    return 0;
}
