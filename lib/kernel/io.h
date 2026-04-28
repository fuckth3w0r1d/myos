#ifndef __LIB__KERNEL_IO_H
#define __LIB_KERNEL_IO_H
#include "stdint.h"

// 向端口port写入一个字节
static inline void outb(uint16_t port, uint8_t data)
{
    asm volatile("outb %b0, %w1" :: "a"(data), "Nd"(port));    //input为data，port，约束分别为al寄存器和dx
}

// 将addr处起始的word_cnt个字写入端口port
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt)
{
    asm volatile("cld\n\t"
                  "rep outsw"
                  :"+S"(addr), "+cx"(word_cnt)
                  :"d"(port)
                  :"memory");
    //output为addr和word_cnt,约束分别为si和cx，input包含前两位和后面的port，约束为dx
}

// 将从端口port读入一个字节返回
static inline uint8_t inb(uint16_t port)
{
    uint8_t data;
    asm volatile("inb %w1, %b0" : "=a"(data) : "Nd"(port));
    return data;
}

// 将从端口port读入的word_cnt个字写入addr
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt)
{
    //insw是指将从端口port处读入2字节到es:edi指向的内存
    asm volatile("cld\n\t"
                  "rep insw"
                  :"+D"(addr), "+c"(word_cnt)
                  :"d"(port)
                  :"memory");
}

#endif
