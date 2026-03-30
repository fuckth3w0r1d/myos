#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H
#include "global.h"
#define BITMAP_MASK 1

struct bitmap{
    uint32_t bytes_len;
    uint8_t* bits;
};

void _init_bitmap(struct bitmap* btmp); // 初始化位图
_Bool _scan_bitmap_test(struct bitmap* btmp, uint32_t bit_offs); // 检测位图某一位
int _scan_bitmap(struct bitmap* btmp, uint32_t cnt); //在位图中申请连续cnt个位，成功则返回其起始位下标，否则返回-1
void _set_bitmap(struct bitmap* btmp, uint32_t bit_offs, _Bool val); //将位图btmp的bit_offs位设置为val

#endif
