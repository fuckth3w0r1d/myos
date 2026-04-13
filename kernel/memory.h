#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "kernel/bitmap.h"
#include "kernel/list.h"

// 虚拟内存池结构，生成一个实例用于管理内核虚拟内存，同时每个用户进程都会有一个实例
struct _vm_pool{
    struct bitmap pool_bitmap;    //本内存池用到的位图结构，用于管理内存
    uint32_t vaddr_start;      //本内存池的虚拟起始地址
    uint32_t pool_size;
};

typedef _Bool _mem_pool_flag;
#define PF_KERNEL 0 // 属于内核
#define PF_USER 1   // 属于用户

#define PG_P_1 1    //页表项或页目录项存在属性位
#define PG_P_0 0    //页表项或页目录项存在属性位
#define PG_RW_R 0   //R/W属性位值，读/执行
#define PG_RW_W 2   //R/W属性位值，读/写/执行
#define PG_US_S 0   //U/S属性位值，系统级
#define PG_US_U 4   //U/S属性位值，用户级

#define PG_SIZE 4096

// 内存块
struct chunk{
    struct list_elem free_elem;
};

// 内存块描述符，一个描述符描述对应的arena
struct _chunk_desc{
    uint32_t chunk_size;  //内存块大小
    uint32_t chunks_per_arena;    //本arena中可容纳此chunk的数量
    struct list free_list;        //目前可用的chunk链表
};

#define DESC_CNT 7              //内存块描述符个数. 16, 32, 64, 128, 256, 512, 1024字节这几种规格

void* alloc_kernel_pages(uint32_t pg_cnt);
void* alloc_user_pages(uint32_t pg_cnt);
void* map_page(_mem_pool_flag pf, uint32_t vaddr);
uint32_t vaddr2paddr(uint32_t vaddr);
void _init_mem(void);
void _init_chunk_desc(struct _chunk_desc* desc_array);
void* sys_malloc(uint32_t size);
void _vm_free_pages(_mem_pool_flag pf, void* _vaddr, uint32_t pg_cnt);
void sys_free(void* ptr);

#endif
