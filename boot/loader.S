%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR 
; ============ 从 mbr 接管，跳转到 loader_start
jmp loader_start  ; 这条指令长 3 字节
times 0x8-($-$$) db 0 ; 对齐到 8 字节方便后续数据的访问

; ============== 构建gdt及其内部描述符
;  63                56 55 52 51 48 47 45 44 43           40 39        32
; +------------------+-----+-----+-----+-----+--------------+------------+
; |   Base 31:24     | G   | D/B | L   | AVL | Limit 19:16  | AccessByte |  高四字节含义
; +------------------+-----+-----+-----+-----+--------------+------------+
; |           Base 23:0            |           Limit 15:0               |  低四字节含义
; +--------------------------------+-----------------------------------+
;  31                             16 15                                0
GDT_BASE: dd 0x00000000             ;低4字节
          dd 0x00000000             ;高4字节，无效描述符，防止选择子未初始化
CODE_DESC: dd 0x0000FFFF            ;dd是伪指令，表示define double-word
            dd DESC_CODE_HIGH4      ;代码段描述符
DATA_STACK_DESC: dd 0x0000FFFF      
                  dd DESC_DATA_HIGH4    ;栈段描述符也是数据段描述符, 为了方便共用一个段
VIDEO_DESC: dd 0x80000007;limit=(0xbffff-0xb8000)/4k=0x7，这里的0xb8000~0xbffff是实模式下文本模式显存适配器的内存地址，因此段界限即为上述方程
            dd DESC_VIDEO_HIGH4 ;此时dpl为0，此乃显存段描述符

GDT_SIZE equ $ - GDT_BASE           ;计算当前GDT已经填充的大小
GDT_LIMIT equ GDT_SIZE - 1          ;
times 60 dq 0           ;此处预留60个描述符空位，这里跟上面一致，表示define quad-word ,也就是定义60个以0填充的段描述符,这里的times是循环次数

; ======== 定义选择子，段选择子 = 索引(index) + 表类型(TI) + 特权级(RPL)
SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0   ;相当于(CODE_DESC - GDT_BASE)/8 + TI_GDT + RPL0
SELECTOR_DATA equ (0x0002<<3) + TI_GDT + RPL0   ;同上类似
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0   ;同上类似
  
; ========= 定义gdt的指针，前2字节是gdt的界限，后4字节是gdt的起始地址
gdt_ptr dw GDT_LIMIT    ;define word
        dd GDT_BASE


; =========== ARDS 结构体示例
; struct ARDS {
;     uint64_t BaseAddr;   // 基地址
;     uint64_t Length;     // 长度
;     uint32_t Type;       // 类型
; };

; =========== ards_buf 用于做 ards 结构体的缓存区
ards_buf times (0x300-2)-($-$$) db 0 ;这个 buf 的偏移是 LOADER_BASE_ADDR + 0x20e, 这下面又定义2字节的ards_nr, 所以对齐一下让后续的total_mem_bytes恰好偏移0x300
ards_nr dw 0        ;用于记录 ards 结构体的数量

; =========== total_mem_bytes用于保存内存容量
;将来在内核中我们会引用到这个地址，偏移 LOADER_BASE_ADDR + 0x300
total_mem_bytes dd 0

loader_start:
    ; ======== int 0x15 eax = 0xe820, edx = 534D4150('SMAP') 获取内存布局
    ; | 调用或返回 | 寄存器/标志 | 参数         | 用途        
    ; | 调用前输入 | EAX    | 0xE820           | 子功能号      
    ; | 调用前输入 | EBX    | 0（第一次）       | ARDS 后续值  
    ; | 调用前输入 | ECX    | 20（或更大）      | ARDS 结构大小 
    ; | 调用前输入 | EDX    | 0x534D4150       | “SMAP”签名  
    ; | 调用前输入 | ES:DI  | 缓冲区地址        | 接收 ARDS   
    ; | 返回后输出 | 寄存器/标志 | 参数         | 用途      
    ; | 返回后输出 | CF     | 0=成功，1=失败    | 调用状态   
    ; | 返回后输出 | EAX    | 0x534D4150       | 校验签名    
    ; | 返回后输出 | ES:DI  | ARDS 结构        | 已填充内存信息 
    ; | 返回后输出 | ECX    | ≥20              | 实际写入字节数 
    ; | 返回后输出 | EBX    | 下一个 ARDS      | 为0表示结束  
    xor ebx, ebx ; 初次 ebx 置零
    mov edx, 0x534d4150
    mov di, ards_buf ; 获取到的内存信息写入准备好的 ards_buf
    .e820_mem_get_loop:
    mov eax, 0x0000e820   ;每次执行int 0x15之后，eax会变成0x534d4150,所以每次执行int之前都要更新为子功能号
    mov ecx, 20 ;ards结构大小为 20 字节
    int 0x15
    jc .e820_failed_so_try_e801 ; 如果发生错误则尝试 0xe801 子功能
    add di, cx ; 跳过ards_buf中已经写入的内存信息
    inc word[ards_nr] ; 记录 ards 结构数量
    cmp ebx, 0  ;若ebx为0且cf不为1,这说明ards全部返回
    jnz .e820_mem_get_loop
    mov cx, word[ards_nr] ; 遍历所有 ards 结构, cx 赋值为ards数量
    mov ebx, ards_buf ; ebx 指向 ards_buf
    xor edx, edx      ; edx 存储最大内存容量，先清零
    .find_max_mem_area:
    mov eax, [ebx]
    add eax, [ebx+8] ; eax = BaseAddrLow + LengthLow, 为内存大小
    add ebx, 20 ; ebx 指向下一个 ards
    cmp edx, eax ; 比较
    jge .next_ards        ;大于或等于
    mov edx, eax          ; 当前内存大小(eax) 更大则更新 edx 为更大的内存大小
    .next_ards:
    loop .find_max_mem_area ;循环，以cx为循环次数
    jmp .mem_get_ok

    ; ========= int 0x15 ax=0xe801 , 获取内存大小，最大支持4G
    ; | 调用或返回 | 寄存器/标志 | 参数          | 用途   |
    ; | 调用前输入 | AX         | 0xE801        | 子功能号 
    ; | 返回后输出 | 寄存器/标志 | 参数                     | 用途          
    ; | 返回后输出 | CF         | 0=成功，1=失败            | 状态          
    ; | 返回后输出 | AX         | 1MB~16MB 内存（KB为单位） | 低端扩展内存      
    ; | 返回后输出 | BX         | 同 AX（备用）             | 有些 BIOS 用这个 
    ; | 返回后输出 | CX         | 16MB以上内存（64KB为单位） | 高端内存        
    ; | 返回后输出 | DX         | 同 CX（备用）             | 有些 BIOS 用这个 
    .e820_failed_so_try_e801:
    mov ax, 0xe801
    int 0x15
    jc .e801_failed_so_try88  ;若cf位为1则说明有错误发生，尝试下一个88方法
    ; == 先算出低15MB的内存
    ; ax和cx中是以KB为单位的内存数量，因此我们将其转换为以byte为单位
    mov cx, 0x400  
    mul cx        ;由于处于实模式，所以mul指令是ax × cx，生成乘数高16位在dx中，低16位存于ax中
    shl edx, 16   
    and eax, 0x0000ffff   
    or edx, eax   ;将乘积保存至edx中
    add edx, 0x100000 ;80286 拥有 24 位地址线，其寻址空间是 16MB。当时有一些 ISA 设备要用15MB~16MB作为缓冲区，操作系统不可以用此段内存空间。只是为了兼容，现在加回来
    mov esi, edx      ;这里保存一下edx的值，后续计算乘法 edx 会被破坏
    ; == 再将16MB以上的内存转换为byte为单位
    xor eax, eax
    mov ax, bx
    mov ecx, 0x10000  ;0x10000为16进制的64K
    mul ecx           ;32位乘法，其高32位和低32位存放在edx和eax中
    add esi, eax      ;由于这里只能最大测出4GB，edx的值肯定为0，所以咱们只需要eax就可以了
    mov edx, esi      ;edx为总内存大小
    jmp .mem_get_ok

    ; ========= int 15h ah=0x88 获取内存大小，只能获取64MB之内
    ; | 调用或返回 | 寄存器/标志 | 参数   | 用途   
    ; | 调用前输入 | AH         | 0x88   | 子功能号
    ; | 返回后输出 | 寄存器/标志 | 参数             | 用途     
    ; | 返回后输出 | CF         | 0=成功，1=失败    | 状态     
    ; | 返回后输出 | AX         | 1MB以上内存（KB） | 扩展内存大小 
    .e801_failed_so_try88:
    ;int 15h后，ax存入的是以KB为单位的内存容量
    mov ah, 0x88
    int 0x15
    jc .error_hlt ; 如果连这个最后的 0x88 子功能也失败，则直接挂起
    and eax, 0x0000ffff
    ;16位乘法
    mov cx, 0x400
    mul cx
    shl edx, 16
    or edx, eax
    add edx, 0x100000  ; 0x88子功能只会返回1MB以上的内存，所以最终我们还需要加上1MB
    .error_hlt:
    jmp $ ;挂起

    ; =========== 内存信息获取成功
    .mem_get_ok:
    mov [total_mem_bytes], edx            ;将内存大小换为bytes为单位(已经在edx中)，存入total_mem_bytes

    ; =========== 准备进入保护模式 
    cli         ; 关闭中断, 因为进入保护模式后 BIOS 的中断向量表不再有效, 如果发生时钟中断则有可能导致异常现象
    ; ===== 打开A20
    in al, 0x92 ; 0x92 是 A20 地址线的控制端口
    or al, 0000_0010B
    out 0x92, al
    ; ====== 加载gdt
    lgdt [gdt_ptr]
    ; ======= 将CR0的PE位置1
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax
    ; ========== 已经进入保护模式
    jmp dword SELECTOR_CODE:p_mode_start ; 刷新流水线 使用dword(32位)防止保护模式下执行这条指令导致地址编码错误

[bits 32]
p_mode_start:
    mov ax, SELECTOR_DATA ; 初始化数据段和栈段基址寄存器
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, LOADER_STACK_TOP ; 初始化栈顶
    mov ax, SELECTOR_VIDEO ; 初始化显存基址寄存器
    mov gs, ax

    mov byte [gs:0x10], 'P' ; 打印提示表明顺利进入保护模式
    mov byte [gs:0x11], 0x0F

    ; ================= 从硬盘读入 kernel
    mov eax, KERNEL_START_SECTOR ;kernel.bin所在的扇区号
    mov ebx, KERNEL_BIN_BASE_ADDR     ;从磁盘读出后，写入到ebx指定的地址
    mov ecx, 200                      ;读入的扇区数
    call rd_disk_m_32                 

    ; ================== 准备进入虚拟地址空间
    ; ==== 创建页目录及页表
    call setup_page

    ; ==== 将描述符表地址及偏移量写入内存 gdt_ptr 后续用虚拟地址重新加载
    sgdt [gdt_ptr]      ; 使用sgdt更新gdt_ptr是为了确保不出错（虽然原来已经自己定义好了）

    mov ebx, [gdt_ptr + 2]        ;加上2是因为gdt_ptr的低2字节是偏移量，高四字节才是GDT地址
    or dword [ebx + 0x18 + 4], 0xc0000000   
    ;段描述符高四字节的最高位是段基址的第31～24位

    ;将gdt的基址加上0xc0000000使其成为内核所在的高地址
    add dword [gdt_ptr + 2], 0xc0000000
    
    add esp, 0xc0000000   ;将栈指针同样映射到内核地址

    ;把页目录地址附给cr3
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax

    ;=打开cr0的pg位（第31位）
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ;在开启分页后，用gdt新的地址重新加载
    lgdt [gdt_ptr]    ;重新加载GDT

    mov byte [gs:0x20], 'V' ; 打印提示表明顺利开启分页进入虚拟地址空间
    mov byte [gs:0x21], 0x0F

    ; =============== 进入kernel运行, loader任务结束
    ;这里是因为一直处于32位之下，但是为了以防万一所以还是加上一个流水线刷新
    jmp SELECTOR_CODE:enter_kernel        ;强制刷新流水线，更新gdt
    enter_kernel:
    call kernel_init
    mov esp, 0xc009f000   ; 设置内核栈
    jmp KERNEL_ENTRY_POINT        ;进入内核

    jmp $
; =========
; =========
; =========
; =========

    ; ================= 将kernel.bin中的segment拷贝到编译的地址
    kernel_init:   
    xor eax, eax
    xor ebx, ebx      ;ebx用来记录程序头表地址
    xor ecx, ecx      ;cx记录程序头表中的program header 数量
    xor edx, edx      ;dx记录program header的尺寸，即e_phentsize

    ; kernel.bin与此前的 mbr 和 loader 不同，它是一个 ELF 文件而非纯二进制文件
    ; typedef struct {
    ;     unsigned char e_ident[16]; /* ELF标识 */
    ;     uint16_t e_type;           /* 文件类型 */
    ;     uint16_t e_machine;        /* 目标架构 */
    ;     uint32_t e_version;        /* ELF版本 */
    ;     uint32_t e_entry;          /* 程序入口虚拟地址 */
    ;     uint32_t e_phoff;          /* 程序头表文件偏移 */
    ;     uint32_t e_shoff;          /* 节头表文件偏移 */
    ;     uint32_t e_flags;          /* 处理器相关标志 */
    ;     uint16_t e_ehsize;         /* ELF头大小 */
    ;     uint16_t e_phentsize;      /* 程序头表项大小 */
    ;     uint16_t e_phnum;          /* 程序头表项数量 */
    ;     uint16_t e_shentsize;      /* 节头表项大小 */
    ;     uint16_t e_shnum;          /* 节头表项数量 */
    ;     uint16_t e_shstrndx;       /* 节名字符串表索引 */
    ; } Elf32_Ehdr;

    ; typedef struct {
    ;     uint32_t p_type;   /* 段类型 */
    ;     uint32_t p_offset; /* 段在文件中的偏移 */
    ;     uint32_t p_vaddr;  /* 段的虚拟地址 */
    ;     uint32_t p_paddr;  /* 段的物理地址（通常忽略） */
    ;     uint32_t p_filesz; /* 段在文件中的大小 */
    ;     uint32_t p_memsz;  /* 段在内存中的大小 */
    ;     uint32_t p_flags;  /* 段标志（读/写/执行） */
    ;     uint32_t p_align;  /* 对齐 */
    ; } Elf32_Phdr;

    mov dx, [KERNEL_BIN_BASE_ADDR + 42]   ;e_phentsize
    mov ebx, [KERNEL_BIN_BASE_ADDR + 28]  ;e_phoff
    add ebx, KERNEL_BIN_BASE_ADDR 
    mov cx, [KERNEL_BIN_BASE_ADDR + 44]   ;e_phnum

    .each_segment:
    cmp byte [ebx + 0], PT_NULL   ;若p_type等于PT_NULL,说明此program未使用
    je .PTNULL
    ;为函数memcpyu压入参数，参数从右往左依次压入
    ;函数原型类似于memcpy(dst, src, size)
    push dword [ebx + 16]     ;program header中偏移16字节的地方是p_filesz,传入size参数
    mov eax, [ebx + 4]        ;p_offset
    add eax, KERNEL_BIN_BASE_ADDR     ;此时eax就是该段的物理地址(此前我们做了内核地址的同时映射)
    push eax                  ;压入mem_cpy的第二个参数，源地址
    push dword [ebx + 8]      ;压入mem_cpy的第一个参数，目的地址，p_vaddr
    call mem_cpy
    add esp, 12               ;清理栈中压入的三个参数
    .PTNULL:
    add ebx, edx              ;edx为program header的尺寸，这里就是跳入下一个描述符
    loop .each_segment 
    ret

    ; =========== 逐字节拷贝 mem_cpy(dst, src, size)
    ;输入：栈中三个参数
    ;输出：无
    mem_cpy:
    cld                       ;控制eflags寄存器中DF = 0， 使得movsb指令向前递增拷贝
    push ebp
    mov ebp, esp  ;构造栈帧
    push ecx      ;rep指令用到了ecx，但ecx对于外层段的循环还有用，所以入栈备份
    mov edi, [ebp + 8]        ;dst
    mov esi, [ebp + 12]       ;src
    mov ecx, [ebp + 16]       ;size
    rep movsb                 ;逐字节拷贝,其中movs代表move string，其中源地址保存在esi，目的地址保存在edi中，其中edi和esi肯定会一直增加，而这个增加的功能由cld指令实现
    ;这里的rep指令是repeat的意思，就是重复执行movsb，循环次数保存在ecx中

    ;恢复环境
    pop ecx                   ;因为外层ecx保存的是程序段数量，这里又要用作size，所以进行恢复
    pop ebp
    ret


    ; ========================= 从硬盘读取数据, 同 mbr.S 中读取loader使用的函数, 只不过在保护模式进行
    rd_disk_m_32:  
    ;eax = 扇区LBA地址
    ;ebx = 将数据写入的内存地址
    ;ecx = 读入的扇区数
    ; ======== 保存参数
    mov esi, eax
    mov edi, ecx
    ; ======== 设置读取的扇区数
    mov dx, 0x1f2  ; 0x1f2 是硬盘控制器的扇区计数寄存器端口
    mov al, cl
    out dx, al
    ; 恢复eax
    mov eax, esi
    ; ========= 将LBA地址写入端口
    ; 低8位
    mov dx, 0x1f3
    out dx, al
    ; 中8位
    mov dx, 0x1f4
    shr eax, 8
    out dx, al
    ; 次高8位
    mov dx, 0x1f5
    shr eax, 8
    out dx, al
    ; 最高4位和设备编码（各4位） 
    mov dx, 0x1f6
    shr eax, 8
    and al, 0x0f
    or al, 0xe0 ; 1110 表示LBA模式
    out dx, al
    ; ========== 向0x1f7端口写入读命令 0x20
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al
    ; ========= 开始循环读取
    mov ecx, edi          ; 外层循环：扇区数
    .read_sector:
    push ecx ; 保存外层循环索引 cx
    .disk_not_ready:
    nop
    mov dx, 0x1f7
    in al, dx
    and al, 0x88  ; 10001000
    cmp al, 0x08  ; 00001000 硬盘不忙且可以读写数据
    jnz .disk_not_ready  ; 等待当前扇区就绪
    mov ecx, 0x100          ; 内层：读256个字 = 512字节, 读一个扇区
    mov dx, 0x1f0
    .continue_read:
    in ax, dx
    mov [ebx], ax
    add ebx, 2
    loop .continue_read
    pop ecx
    loop .read_sector
    ret

    ; =================================== 创建内核页表和页目录表
    setup_page:
    ; 虚拟地址32位划分
    ; [10位][10位][12位]
    ; ↓     ↓     ↓
    ; 目录   页表   页内偏移

    ; ==== 先把页目录表占用的空间逐字节清0
    mov ecx, 0x1000     
    mov esi, 0
    .clear_page_dir:
    mov byte [PAGE_DIR_TABLE_POS + esi], 0
    inc esi
    loop .clear_page_dir
    ; ==== 开始创建内核地址空间最低4MB的页目录项(PDE)
    .create_pde:
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x1000 ;页目录表本身占用0x1000大小，+0x1000就是第一个页表的物理地址
    mov ebx, eax    ;此处为ebx赋值， 是为.create_pte做准备， ebx为基址
    or eax, PG_US_U | PG_RW_W | PG_P ;页目录项的属性RW和P位为1,US为1表示用户属性，所有特权级都可以访问
    mov [PAGE_DIR_TABLE_POS + 0x0], eax       ;第一个目录项
    ; 将物理地址的最低的 4MB 映射到虚拟地址最低的 4MB, 为了在开启分页后loader能够继续正常执行
    mov [PAGE_DIR_TABLE_POS + 0xc00], eax     ;一个页目录项占用4字节, +0xc00对应第0x300个页目录项
    ; 一个页目录项对应一张页表，一张页表大小0x1000字节, 一个页表项4字节, 共记录0x400页, 一页大小也是0x1000
    ; 那么第0x300个页目录项也就对应地址 0x300*0x400*0x1000 = 0xc0000000(内核地址的起始) 开始的 4MB 内存
    ; 也就将物理地址的最低的 4MB 映射到了虚拟地址空间中 内核地址最低的 4MB 
    sub eax, 0x1000
    mov [PAGE_DIR_TABLE_POS + 4092], eax      ;使得最后一个目录项地址指向页目录表自己的地址

    ; ==== 开始创建内核地址空间最低4MB对应的页表项(PTE)
    mov ecx, 256                  ;1M低端内存/每页大小4K = 256, 因为物理地址最低的4MB只使用最低的1MB内存来存放kernel, 所以内核地址对应的的页表只需创建 256 页
    mov esi, 0                    ;虚拟地址0x0～0x3fffff和虚拟地址0xc0000000~0xc03fffff对应的物理页，现在只用了低1MB，此时虚拟地址是等于物理地址的
    mov edx, PG_US_U | PG_RW_W | PG_P     ; 页表属性。注意进入循环前第一页物理地址是0起始，所以直接用属性赋值即可
    .create_pte:    ;创建Page Table Entry
    mov [ebx + esi*4], edx    ;ebx为第一个页表的首地址，此前已赋值
    add edx, 0x1000 ; 下一页
    inc esi
    loop .create_pte

    ; ==== 创建内核地址空间其他页面的PDE (内核占用1GB, 此前只创建了最低的4MB的PDE)
    mov eax, PAGE_DIR_TABLE_POS 
    add eax, 0x2000       ;此时eax为第二张页表的物理地址
    or eax, PG_US_U | PG_RW_W | PG_P  ;页表属性
    mov ebx, PAGE_DIR_TABLE_POS 
    mov ecx, 254          ;范围为第0x301～0x3fe的所有页目录项数量(第0x3ff项是页目录表自身的物理地址)
    mov esi, 769
    .create_kernel_pde:
    mov [ebx+esi*4], eax
    inc esi
    add eax, 0x1000 ; 下一张页表
    loop .create_kernel_pde
    ; 至此就将整个内核地址空间(1GB)都映射到了物理地址最低的1GB
    ret