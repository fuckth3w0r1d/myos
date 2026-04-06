#include "memory.h"
#include "stdint.h"
#include "stdtype.h"
#include "global.h"
#include "debug.h"
#include "string.h"
#include "memfunc.h"
#include "sync.h"
#include "kernel/bitmap.h"
#include "kernel/print.h"

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)       //这里是获取虚拟地址前十位，这里就是PDE的索引值
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)       //这里是获取虚拟地址中间十位，这里是PTE的索引值
#define K_HEAP_START 0xc0100000         //设置堆起始地址用来进行动态分配 （跳过内核本身所在的0xc0000000-0xc0100000）
#define MEM_BITMAP_BASE 0xc007e000      //0xc009f000 是内核主线程栈顶，0xc009e000 是内核主线程的 pcb
                                        // 一页长度位图可管理128MB内存，这里兼容32位机最大寻址4GB，使用32页放位图
                                        // 避免麻烦这里要使用0xc0100000以内地址，因为内核空间其他地址还没映射

// 物理内存池结构，生成两个实例用于管理内核内存池和用户内存池
struct _pm_pool{
    struct bitmap pool_bitmap;    //本内存池用到的位图结构，用于管理内存
    uint32_t paddr_start;      //本内存池的物理起始地址
    uint32_t pool_size;
    struct lock lock;          // 申请内存时互斥
};

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
static void* _get_pages_vaddr(_mem_pool_flag pf, uint32_t pg_cnt)
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
        //用户内存池
        struct _task_struct* cur = running_thread();
        bit_idx_start = _scan_bitmap(&cur->user_vm.pool_bitmap, pg_cnt);     //查找当前线程的虚拟用户内存池
        if(bit_idx_start == -1) return NULL;
        while(cnt < pg_cnt)
        {
            _set_bitmap(&cur->user_vm.pool_bitmap, bit_idx_start + cnt, 1);
            cnt++;
            // (0xc0000000 - PG_SIZE)作为用户3级栈已经在start_process被分配
            ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
        }
        vaddr_start = cur->user_vm.vaddr_start + bit_idx_start * PG_SIZE;
    }
    return (void*)vaddr_start;
}

// 在m_pool指向的物理内存池中分配1页，成功则返回页的物理地址，失败则返回NULL
static void* _pm_alloc_page(struct _pm_pool* m_pool)
{
    int bit_idx = _scan_bitmap(&m_pool->pool_bitmap, 1);   //找一个物理页面
    if(bit_idx == -1) return NULL;
    _set_bitmap(&m_pool->pool_bitmap, bit_idx, 1);         //将对应位置1
    uint32_t page_paddr = ((bit_idx * PG_SIZE) + m_pool->paddr_start);
    return (void*)page_paddr;
}

// 页表中添加虚拟地址_vaddr与物理地址_page_paddr的映射
static void _pt_map_page(void* _vaddr, void* _page_paddr)
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
        uint32_t pde_paddr = (uint32_t)_pm_alloc_page(&kernel_pm); //页表本身是一页，这一页从内核内存分配
        *pde = (pde_paddr | PG_US_U | PG_RW_W | PG_P_1);
        // 分配到的物理页地址pde_phyaddr对应的物理内存清0,
        memset((void*)((uint32_t)pte & 0xfffff000), 0, PG_SIZE); // 注意这里要用虚拟地址传参, 所以使用(uint32_t)pte & 0xfffff000
        ASSERT(!(*pte & 0x00000001));
        *pte = (page_paddr | PG_US_U | PG_RW_W | PG_P_1);
    }
}

// 在虚拟内存分配pg_cnt个页，成功则返回起始虚拟地址，失败时则返回NULL
static void* _vm_alloc_pages(_mem_pool_flag pf, uint32_t pg_cnt)
{
    ASSERT(pg_cnt > 0 && pg_cnt < 32512);
    // 在虚拟内存池中申请连续的虚拟页
    void* vaddr_start = _get_pages_vaddr(pf, pg_cnt);
    if(vaddr_start == NULL) return NULL;
    uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
    struct _pm_pool* mem_pool = (pf == PF_KERNEL) ? &kernel_pm : &user_pm;
    //逐页分配物理内存页并映射
    while(cnt > 0)
    {
        void* page_paddr = _pm_alloc_page(mem_pool);
        if(page_paddr == NULL) return NULL;
        _pt_map_page((void*)vaddr, page_paddr);     //映射
        vaddr += PG_SIZE;         //下一个虚拟页
        cnt--;
    }
    return vaddr_start;
}

// 映射分配指定地址vaddr, 仅支持一页空间分配
void* map_page(_mem_pool_flag pf, uint32_t vaddr)
{
    struct _pm_pool* mem_pool = (pf == PF_KERNEL) ? &kernel_pm : &user_pm;
    lock_acquire(&mem_pool->lock);
    // 先将虚拟地址对应的位图置1
    struct _task_struct* cur = running_thread();
    int32_t bit_idx = -1;
    if(cur->pgdir != NULL && pf == PF_USER)
    { // 若当前是用户进程申请用户内存，就修改用户进程自己的虚拟地址位图
        bit_idx = (vaddr - cur->user_vm.vaddr_start)/PG_SIZE;
        ASSERT(bit_idx > 0);
        _set_bitmap(&cur->user_vm.pool_bitmap, bit_idx, 1);
    }else if(cur->pgdir == NULL && pf == PF_KERNEL){
        // 如果当前是内核线程申请内核内存，则修改kernel_vm
        bit_idx = (vaddr - kernel_vm.vaddr_start)/PG_SIZE;
        ASSERT(bit_idx > 0);
        _set_bitmap(&kernel_vm.pool_bitmap, bit_idx, 1);
    }else{
        PANIC("map_page:not allow kernel alloc userspace or user alloc kernelspace by map_page");
    }

    void* paddr = _pm_alloc_page(mem_pool);
    if(paddr == NULL)
    {
        lock_release(&mem_pool->lock);
        return NULL;
    }
    // 映射到物理页
    _pt_map_page((void*)vaddr, paddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

// 封装一个分配内核虚拟内存页的函数, 成功返回虚拟地址否则NULL
void* alloc_kernel_pages(uint32_t pg_cnt)
{
    lock_acquire(&kernel_pm.lock);  // 上锁
    void* vaddr = _vm_alloc_pages(PF_KERNEL, pg_cnt);
    if(vaddr != NULL) memset(vaddr, 0, pg_cnt * PG_SIZE); //如果分配的地址不为空，则将页框清0后返回
    lock_release(&kernel_pm.lock);
    return vaddr;
}
// 封装一个分配用户虚拟内存页的函数, 成功返回虚拟地址否则NULL
void* alloc_user_pages(uint32_t pg_cnt)
{
    lock_acquire(&user_pm.lock); // 用户进程基于内核线程实现, 需要上锁
    void* vaddr = _vm_alloc_pages(PF_USER, pg_cnt);
    if(vaddr != NULL) memset(vaddr, 0, pg_cnt * PG_SIZE); //如果分配的地址不为空，则将页框清0后返回
    lock_release(&user_pm.lock);
    return vaddr;
}

// 得到虚拟地址映射到的物理地址
uint32_t vaddr2paddr(uint32_t vaddr)
{
    uint32_t* pte = pte_ptr(vaddr);
    // (*pte)的值是页表所在的物理页框的地址， 去掉其低12位的页表项属性 + 虚拟地址vaddr的低12位页内偏移
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
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

    // 初始化内存池锁
    lock_init(&kernel_pm.lock);
    lock_init(&user_pm.lock);

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
