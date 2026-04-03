#include "memory.h"
#include "stdint.h"
#include "stdtype.h"
#include "global.h"
#include "debug.h"
#include "string.h"
#include "memfunc.h"
#include "kernel/bitmap.h"
#include "kernel/print.h"

#define PG_SIZE 4096
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)       //这里是获取虚拟地址前十位，这里就是PDE的索引值
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)       //这里是获取虚拟地址中间十位，这里是PTE的索引值
#define K_HEAP_START 0xc0100000         //设置堆起始地址用来进行动态分配 （跳过内核本身所在的0xc0000000-0xc0100000）
#define MEM_BITMAP_BASE 0xc007e000      //0xc009f000 是内核主线程栈顶，0xc009e000 是内核主线程的 pcb
                                        // 一页长度位图可管理128MB内存，这里兼容32位机最大寻址4GB，使用32页放位图
                                        // 避免麻烦这里要使用0xc0100000以内地址，因为内核空间其他地址还没映射呢
struct _pm_pool user_pm, kernel_pm;
struct _vm_pool kernel_vm;

// 得到虚拟地址vaddr对应的页表项的指针
static uint32_t* pte_ptr(uint32_t vaddr)
{
    //根据loader建立的映射, 0xffc00000对应的是页目录表的第0项
    //用页目录索引乘以页大小（4KB）跳转到第 PDE_IDX(vaddr) 张页表
    //用页表索引乘以4（每个页表项4字节） 找到第 PTE_IDX(vaddr) 个页表项
    uint32_t* pte = (uint32_t*)(0xffc00000 | (PDE_IDX(vaddr) << 12) | (PTE_IDX(vaddr) << 2));
    return pte;
}

// 得到虚拟地址vaddr对应的页目录项的指针
static uint32_t* pde_ptr(uint32_t vaddr)
{
    //根据loader建立的映射, 0xfffff000就是页目录表本身的虚拟地址
    //用页目录索引乘以页目录项大小（4B），找到第 PDE_IDX(vaddr) 个页目录项
    uint32_t* pde = (uint32_t*)(0xfffff000 | (PDE_IDX(vaddr) << 2));
    return pde;
}


// 在对应的虚拟内存池中分配pg_cnt个虚拟页地址，成功则返回虚拟页的起始地址，失败则返回NULL (仅分配地址)
static void* _get_vaddr(_mem_pool_flag pf, uint32_t pg_cnt)
{
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if(pf == PF_KERNEL)
    {
        bit_idx_start = _scan_bitmap(&kernel_vm.pool_bitmap, pg_cnt);  //先查找位图看是否有连续的足够大的内存
        if(bit_idx_start == -1) return NULL;
        while(cnt < pg_cnt)
        {
            _set_bitmap(&kernel_vm.pool_bitmap, bit_idx_start + cnt, 1);
            cnt++;
        }
        vaddr_start = kernel_vm.vaddr_start + bit_idx_start * PG_SIZE;
    }else{
        //用户内存池，之后再补充
    }
    return (void*)vaddr_start;
}

// 在m_pool指向的物理内存池中分配1页，成功则返回页的物理地址，失败则返回NULL
static void* _palloc_1_p(struct _pm_pool* m_pool)
{
    int bit_idx = _scan_bitmap(&m_pool->pool_bitmap, 1);   //找一个物理页面
    if(bit_idx == -1) return NULL;
    _set_bitmap(&m_pool->pool_bitmap, bit_idx, 1);         //将对应位置1
    uint32_t page_paddr = ((bit_idx * PG_SIZE) + m_pool->paddr_start);
    return (void*)page_paddr;
}

// 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射
static void _new_page(void* _vaddr, void* _page_paddr)
{
    uint32_t vaddr = (uint32_t)_vaddr, page_paddr = (uint32_t)_page_paddr;
    uint32_t* pde = pde_ptr(vaddr);
    uint32_t* pte = pte_ptr(vaddr);
    // 先在页目录内判断目录项的P位，若为1则表示该表已经存在
    if(*pde & 0x00000001)
    {
        //页目录项和页表项的第0位为p，这里是判断页目录项是否存在
        ASSERT(!(*pte & 0x00000001));   //已经装载的物理页，则报错
        if(!(*pte & 0x00000001))
        {
            *pte = (page_paddr | PG_US_U | PG_RW_W | PG_P_1);
        }else{
            PANIC("pte repeat");
            *pte = (page_paddr | PG_US_U | PG_RW_W | PG_P_1);
        }
    }else{
        //页目录项不存在，所以需要先创建页目录再创建页表项
        uint32_t pde_paddr = (uint32_t)_palloc_1_p(&kernel_pm); //页表本身是一页，这一页从内核内存分配
        *pde = (pde_paddr | PG_US_U | PG_RW_W | PG_P_1);
        // 分配到的物理页地址pde_phyaddr对应的物理内存清0,
        memset((void*)((uint32_t)pte & 0xfffff000), 0, PG_SIZE); // 注意这里要用虚拟地址传参, 所以使用(uint32_t)pte & 0xfffff000
        ASSERT(!(*pte & 0x00000001));
        *pte = (page_paddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

// 在虚拟内存分配pg_cnt个页，成功则返回起始虚拟地址，失败时则返回NULL
void* _valloc_p(_mem_pool_flag pf, uint32_t pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 32512);
    // 在虚拟内存池中申请连续的虚拟页
    void* vaddr_start = _get_vaddr(pf, pg_cnt);
    if(vaddr_start == NULL) return NULL;
    uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
    struct _pm_pool* mem_pool = (pf == PF_KERNEL) ? &kernel_pm : &user_pm;
    //逐页分配物理内存页并映射
    while(cnt > 0)
    {
        void* page_paddr = _palloc_1_p(mem_pool);
        if(page_paddr == NULL) return NULL;
        _new_page((void*)vaddr, page_paddr);     //映射
        vaddr+=PG_SIZE;         //下一个虚拟页
        cnt--;
    }
    return vaddr_start;
}

// 封装一个分配内核虚拟内存页的函数, 成功返回虚拟地址否则NULL
void* alloc_kernel_pages(uint32_t pg_cnt)
{
    void* vaddr = _valloc_p(PF_KERNEL, pg_cnt);
    if(vaddr != NULL) memset(vaddr, 0, pg_cnt * PG_SIZE); //如果分配的地址不为空，则将页框清0后返回
    return vaddr;
}

static void _init_mem_pool(uint32_t all_mem)
{
    print("mem pool init start\n");
    //目前所有页表使用内存大小 = 页目录表本身1页 + 第0项和第768项页目录项指向的同一个页表占1页 + 第769～1022个页目录项占254页，共256页
    uint32_t page_table_used_mem = PG_SIZE * 256;
    uint32_t used_mem = page_table_used_mem + 0x100000;   //0x100000为低端1MB内存
    uint32_t free_mem = all_mem - used_mem;
    uint32_t all_free_pages = free_mem / PG_SIZE;

    uint32_t kernel_free_pages = all_free_pages / 2; // 内核和用户各占一半
    uint32_t user_free_pages = all_free_pages - kernel_free_pages;
    uint32_t kbm_length = (kernel_free_pages+7) / 8;   // 位图长度（向上取整）
    uint32_t ubm_length = (user_free_pages+7) / 8;

    // 物理内存池地址
    uint32_t kp_start = used_mem;     //kernel pool start 内核物理内存池的物理起始地址
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;   //user pool start 用户物理内存池的物理起始地址
    // 接下来初始化物理内存池
    kernel_pm.paddr_start = kp_start;
    user_pm.paddr_start = up_start;

    kernel_pm.pool_size = kernel_free_pages * PG_SIZE;
    user_pm.pool_size = user_free_pages * PG_SIZE;

    kernel_pm.pool_bitmap.bytes_len = kbm_length;
    user_pm.pool_bitmap.bytes_len = ubm_length;

    kernel_pm.pool_bitmap.bits = (void*)(uint32_t)MEM_BITMAP_BASE;
    user_pm.pool_bitmap.bits = (void*)((uint32_t)MEM_BITMAP_BASE + kbm_length);

    // 输出内存池信息
    print("kernel_pool_bitmap_start:");
    print((uint32_t)kernel_pm.pool_bitmap.bits);
    print("    kernel_pool_phy_addr_start:");
    print((uint32_t)kernel_pm.paddr_start);
    print("\n");
    print("user_pool_bitmap_start:");
    print((uint32_t)user_pm.pool_bitmap.bits);
    print("    user_pool_phy_addr_start:");
    print((uint32_t)user_pm.paddr_start);
    print("\n");

    // 将位图置为0
    _init_bitmap(&kernel_pm.pool_bitmap);
    _init_bitmap(&user_pm.pool_bitmap);

    // 下面初始化内核虚拟内存的位图
    kernel_vm.pool_bitmap.bytes_len = kbm_length;         //用于维护内核堆的虚拟地址，所以要和内核内存池大小一致
    kernel_vm.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
    kernel_vm.vaddr_start = K_HEAP_START;
    _init_bitmap(&kernel_vm.pool_bitmap);
    print("     mem_pool_init done \n");
}

void _init_mem(void)
{
    print("mem init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xc0000c00)); // 在 loader 中记录的内存容量
    print("all memory:");
    print(mem_bytes_total);
    print("\n");
    _init_mem_pool(mem_bytes_total);
    print("mem init done!\n");
}
