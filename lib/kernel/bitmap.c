#include "kernel/bitmap.h"
#include "stdint.h"
#include "string.h"
#include "memfunc.h"
#include "kernel/print.h"
#include "interrupt.h"
#include "debug.h"

void _init_bitmap(struct bitmap* btmp)
{
    memset(btmp->bits, 0, btmp->bytes_len);
}

_Bool _scan_bitmap_test(struct bitmap* btmp, uint32_t bit_offs)
{
    uint32_t byte_idx = bit_offs / 8;
    uint32_t bit_idx = bit_offs % 8;
    return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_idx));
}

int _scan_bitmap(struct bitmap* btmp, uint32_t cnt)
{
    uint32_t byte_idx = 0;                    //记录空闲位所在的字节
    // 逐字节比较, 先找到第一个空闲位
    while((0xff == btmp->bits[byte_idx]) && (byte_idx < btmp->bytes_len))
    {
        byte_idx++;
    }
    ASSERT(byte_idx < btmp->bytes_len);
    if(byte_idx == btmp->bytes_len) return -1; // 无剩余空间返回-1
    // 找到了空闲位所在字节，则在该字节内逐位比对，返回空闲位的索引
    int bit_idx = 0;
    while((uint8_t)(BITMAP_MASK << bit_idx) & btmp->bits[byte_idx])
    { //注意这里&是按位与，所有位都为0才返回0,跳出循环
        bit_idx++;
    }

    int bit_offs = byte_idx * 8 + bit_idx;       //这里就是空闲位在位图中的坐标
    if(cnt == 1) return bit_offs;           //若咱们只申请数量为1

    uint32_t bit_remain = (btmp->bytes_len*8 - bit_offs);     //记录还剩下多少个位
    uint32_t next_bit = bit_offs + 1;
    uint32_t count = 1;               //用来记录找到空闲位的个数

    bit_offs = -1;               //将其置-1,若找不到连续的位就返回
    while(bit_remain-- >0)
    {
        if(!(_scan_bitmap_test(btmp, next_bit)))
        {
            count++;
        }else{
            count = 0;
        }
        if(count == cnt)
        {
            bit_offs = next_bit - cnt + 1 ;
            break;
        }
        next_bit++ ;
    }
    return bit_offs;
}

void _set_bitmap(struct bitmap* btmp, uint32_t bit_offs, _Bool val)
{
     ASSERT((val == 0) || (val == 1));
    uint32_t byte_idx = bit_offs / 8;
    uint32_t bit_odd = bit_offs % 8;
    if(val)
    {                                //value为1
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    }else{                                    //value为0
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
    }
}
