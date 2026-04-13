#include "memory.h"
#include "stdint.h"
#include "stdtype.h"
#include "global.h"
#include "debug.h"
#include "string.h"
#include "memfunc.h"
#include "sync.h"
#include "interrupt.h"
#include "kernel/bitmap.h"
#include "kernel/print.h"

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)       //这里是获取虚拟地址前十位，这里就是PDE的索引值
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)       //这里是获取虚拟地址中间十位，这里是PTE的索引值
#define K_HEAP_START 0xc0100000         //设置堆起始地址用来进行动态分配 （跳过内核本身所在的0xc0000000-0xc0100000）
#define MEM_BITMAP_BASE 0xc007e000      //0xc009f000 是内核主线程栈顶，0xc009e000 是内核主线程的 pcb
                                        // 一页长度位图可管理128MB内存，这里兼容32位机最大寻址4GB，使用32页放位图
                                        // 避免麻烦这里要使用0xc0100000以内地址，因为内核空间其他地址还没映射

// 内存仓库
struct arena{
    struct _chunk_desc* desc;  //此arena关联的chunk_desc
    // large为true时，cnt表示的是页框数。否则cnt表示空闲的chunk数量
    uint32_t cnt;
    bool large;
};

struct _chunk_desc ChunkDescs[DESC_CNT];  //内核内存块描述符数组

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

// 页表中添加虚拟地址_vaddr与物理地址_page_paddr的映射 (一页)
static void _pt_add_page(void* _vaddr, void* _page_paddr)
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
        _pt_add_page((void*)vaddr, page_paddr);     //映射
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
    _pt_add_page((void*)vaddr, paddr);
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

// 为malloc做准备
void _init_chunk_desc(struct _chunk_desc* desc_array)
{
    uint16_t desc_idx, chunk_size = 16;
    // 初始化每个chunk描述符
    for(desc_idx = 0; desc_idx < DESC_CNT; desc_idx++)
    {
        desc_array[desc_idx].chunk_size = chunk_size;
        // 初始化arena中的内存块数量
        desc_array[desc_idx].chunks_per_arena = (PG_SIZE - sizeof(struct arena)) / chunk_size;
        list_init(&desc_array[desc_idx].free_list);
        chunk_size *= 2;        //更新为下一个规格内存块
    }
}

void _init_mem(void)
{
    print("mem init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xc0000c00)); // 在 loader 中记录的内存容量
    print("all memory:");
    print(mem_bytes_total);
    print("\n");
    _init_mem_pool(mem_bytes_total);
    _init_chunk_desc(ChunkDescs);
    print("mem init done!\n");
}


// 返回arena中第idx个内存块的地址
static struct chunk* arena2chunk(struct arena* a, uint32_t idx)
{
    return (struct chunk*) ((uint32_t)a + sizeof(struct arena) + idx * a->desc->chunk_size);
}

// 返回内存块b所在的arena地址
static struct arena* chunk2arena(struct chunk* b)
{
    return (struct arena*)((uint32_t)b & 0xfffff000);
}

// 在堆中申请size字节内存
void* sys_malloc(uint32_t size)
{
    _mem_pool_flag PF;
    struct _pm_pool* mem_pool;
    uint32_t pool_size;
    struct _chunk_desc* descs;
    struct _task_struct* cur_thread = running_thread();
    // 判断使用哪个内存池
    if(cur_thread->pgdir == NULL)
    {    //若为内核线程
        PF = PF_KERNEL;
        pool_size = kernel_pm.pool_size;
        mem_pool = &kernel_pm;
        descs = ChunkDescs;
    }else{
        PF = PF_USER;
        pool_size = user_pm.pool_size;
        mem_pool = &user_pm;
        descs = cur_thread->user_chunkdescs;
    }

    // 若申请的内存不再内存池容量范围内，则直接返回NULL
    if(!(size > 0 && size < pool_size)) return NULL;
    struct arena* a;
    struct chunk* b;
    lock_acquire(&mem_pool->lock);
    // 超过最大内存块，就分配页框
    if(size > 1024)
    {
        uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);     //向上取整需要的页框数
        a = _vm_alloc_pages(PF, page_cnt);
        if(a != NULL)
        {
            memset(a, 0, page_cnt * PG_SIZE);     //将分配的内存清0
            // 对于分配的大块页框，将desc置为NULL, cnt置为页框数，large置为true
            a->desc = NULL;
            a->cnt = page_cnt;
            a->large = true;
            lock_release(&mem_pool->lock);
            return (void*)(a + 1);  //跨过arena大小，把剩下的内存返回
        }else{
            lock_release(&mem_pool->lock);
            return NULL;
        }
    }else{    //若申请的内存小于等于1024,则可在各种规格的 chunk_desc 适配
        uint8_t desc_idx;
        // 从内存块描述符中匹配合适的内存块规格
        for(desc_idx = 0; desc_idx < DESC_CNT; desc_idx++)
        {
            if(size <= descs[desc_idx].chunk_size) break;          //从小往大找
        }
        // 若chunk_desc 的free_list中已经没有可用的chunk, 就创建新的arena提供chunk
        if(list_empty(&descs[desc_idx].free_list))
        {
            a = _vm_alloc_pages(PF, 1);       //分配1页框作为arena
            if(a == NULL)
            {
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a, 0, PG_SIZE);
            // 对于分配的小块内存，将desc置为相应内存块描述符，cnt置为此arena可用的内存块数, large置为false
            a->desc = &descs[desc_idx];
            a->large = false;
            a->cnt = descs[desc_idx].chunks_per_arena;
            uint32_t chunk_idx;
            _intr_status old_status = _disable_intr();
            // 开始将arena拆分成内存块，并添加到内存块描述符的free_list当中
            for(chunk_idx = 0; chunk_idx < descs[desc_idx].chunks_per_arena; chunk_idx++)
            {
                b = arena2chunk(a, chunk_idx);
                ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
                list_append(&a->desc->free_list, &b->free_elem);
            }
            _set_intr_status(old_status);
        }
        // 开始分配内存块
        b = elem2entry(struct chunk, free_elem, list_pop(&(descs[desc_idx].free_list)));
        memset(b, 0, descs[desc_idx].chunk_size);
        a = chunk2arena(b);     //获取所在arena
        a->cnt--;
        lock_release(&mem_pool->lock);
        return (void*)b;
    }
}

// 将物理地址pg_phy_addr起始的一页回收到物理内存池
static void _pm_free_page(uint32_t pg_phy_addr)
{
    struct _pm_pool* mem_pool;
    uint32_t bit_idx = 0;
    if(pg_phy_addr >= user_pm.paddr_start)
    {   //用户物理内存池
        mem_pool = &user_pm;
        bit_idx = (pg_phy_addr - user_pm.paddr_start) / PG_SIZE;
    }else{
        mem_pool = &kernel_pm;
        bit_idx = (pg_phy_addr - kernel_pm.paddr_start) / PG_SIZE;
    }
    _set_bitmap(&mem_pool->pool_bitmap, bit_idx, 0);
}

// 去掉页表中虚拟地址vaddr起始一页的映射，只用去掉vaddr对应的pte
static void _pt_remove_page(uint32_t vaddr)
{
    uint32_t* pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1;  //将页表项pte的P位置为0
    asm volatile("invlpg %0" : : "m"(vaddr) : "memory");  //更新tlb，这里因为以前的页表会存在高速缓存，现在咱们修改了所以需要刷新一下tlb对应的条目
}

// 在虚拟地址池当中释放以_vaddr起始的连续pg_cnt个虚拟地址页
static void _free_pages_vaddr(_mem_pool_flag pf, void* _vaddr, uint32_t pg_cnt)
{
    uint32_t bit_idx_start = 0, vaddr = (uint32_t)_vaddr, cnt = 0;
    if(pf == PF_KERNEL)
    {   // 内核
        bit_idx_start = (vaddr - kernel_vm.vaddr_start) / PG_SIZE;
        while(cnt < pg_cnt)
        {
            _set_bitmap(&kernel_vm.pool_bitmap, bit_idx_start + cnt, 0);
            cnt++;
        }
    }else{
        struct _task_struct* cur_thread = running_thread();
        bit_idx_start = (vaddr - cur_thread->user_vm.vaddr_start) / PG_SIZE;
        while(cnt < pg_cnt)
        {
            _set_bitmap(&cur_thread->user_vm.pool_bitmap, bit_idx_start + cnt, 0);
            cnt++;
        }
    }
}

// 释放虚拟地址vaddr为起始的cnt个页
void _vm_free_pages(_mem_pool_flag pf, void* _vaddr, uint32_t pg_cnt)
{
    uint32_t pg_phy_addr;
    uint32_t vaddr = (int32_t)_vaddr, page_cnt = 0;
    ASSERT(pg_cnt >= 1 && vaddr % PG_SIZE == 0);
    pg_phy_addr = vaddr2paddr(vaddr);    //获取虚拟地址vaddr对应的物理地址
    // 确保待释放的物理内存在低端1MB + 1KB大小的页目录 + 1KB大小的页表地址范围外
    ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);
    // 判断pg_phy_addr属于用户物理内存池还是内核物理内存池
    if(pg_phy_addr >= user_pm.paddr_start)
    {  //位于user内存池
        vaddr -= PG_SIZE;
        while(page_cnt < pg_cnt)
        {
            vaddr += PG_SIZE;
            pg_phy_addr = vaddr2paddr(vaddr);
            // 确保此物理地址属于用户物理内存池
            ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= user_pm.paddr_start);
            // 先将对应的物理页框归还到内存池
            _pm_free_page(pg_phy_addr);
            // 再从页表中去除此虚拟地址所在的页表项pte
            _pt_remove_page(vaddr);
            page_cnt++;
        }
        // 清空虚拟地址位图中的相应位
        _free_pages_vaddr(pf, _vaddr, pg_cnt);
    }else{
        vaddr -= PG_SIZE;
        while(page_cnt < pg_cnt)
        {
            vaddr += PG_SIZE;
            pg_phy_addr = vaddr2paddr(vaddr);
            // 确保此物理地址属于内核物理内存池
            ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr < user_pm.paddr_start);
            // 先将对应的物理页框归还到内存池
            _pm_free_page(pg_phy_addr);
            // 再从页表中清楚此虚拟地址所在的页表项pte
            _pt_remove_page(vaddr);
            page_cnt++;
        }
        // 清空虚拟地址位图中的相应位
        _free_pages_vaddr(pf, _vaddr, pg_cnt);
    }
}

// 回收内存ptr
void sys_free(void* ptr)
{
    ASSERT(ptr != NULL);
    if(ptr != NULL)
    {
        _mem_pool_flag PF;
        struct _pm_pool* mem_pool;
        // 判断是内核线程还是用户进程
        if(running_thread()->pgdir == NULL)
        { // 内核线程
            ASSERT((uint32_t)ptr > K_HEAP_START);
            PF = PF_KERNEL;
            mem_pool = &kernel_pm;
        }else{
            PF = PF_USER;
            mem_pool = &user_pm;
        }
        lock_acquire(&mem_pool->lock);
        struct chunk* b = ptr;
        struct arena* a = chunk2arena(b);
        //把chunk换成arena，获取元信息
        ASSERT(a->large == 0 || a->large == 1);
        if(a->desc == NULL && a->large == true)
        {    //大于1024的内存是直接分配的页
            _vm_free_pages(PF, a, a->cnt);
        }else{                          //小于1024的内存
            // 先将内存块回收到free_list
            list_append(&a->desc->free_list, &b->free_elem);
            a->cnt++;
            // 再判断arena中的块是否都空闲，若是则收回整个arena
            if(a->cnt == a->desc->chunks_per_arena)
            {
                uint32_t chunk_idx;
                for(chunk_idx = 0; chunk_idx < a->desc->chunks_per_arena; chunk_idx++)
                {
                    struct chunk* b = arena2chunk(a, chunk_idx);
                    ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
                    list_remove(&b->free_elem);
                }
                _vm_free_pages(PF, a, 1);
            }
        }
        lock_release(&mem_pool->lock);
    }
}
