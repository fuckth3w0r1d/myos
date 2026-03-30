#ifndef __LIB_MEMORY_H
#define __LIB_MEMORY_H
#include "stdint.h"
void memset(void* dst, uint8_t val, uint32_t size); //单字节复制，指定目的地
void memcpy(void* dst, const void* src, uint32_t size);     //多字节复制，指定目的地
int memcmp(const void* a_, const void* b_, uint32_t size);  //比较多个字节
#endif
