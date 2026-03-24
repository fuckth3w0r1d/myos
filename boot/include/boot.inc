; ================ loader 和 kernel 
LOADER_BASE_ADDR equ 0x900     ;Loader存放内存首址
LOADER_STACK_TOP equ LOADER_BASE_ADDR
LOADER_START_SECTOR equ 0x2     ;Loader存放硬盘扇区
PAGE_DIR_TABLE_POS equ 0x100000     ;页目录存放首址
KERNEL_START_SECTOR equ 0x9         ;Kernel存放硬盘扇区
KERNEL_BIN_BASE_ADDR equ 0x70000    ;Kernel存放内存首址
KERNEL_ENTRY_POINT equ 0xc0001500   ;Kernel程序入口地址

; =================== gdt描述符属性 

; ===================== 段描述符定义 =====================
;  63                56 55 52 51 48 47 45 44 43           40 39        32
; +------------------+-----+-----+-----+-----+--------------+------------+
; |   Base 31:24     | G   | D/B | L   | AVL | Limit 19:16  | AccessByte |
; +------------------+-----+-----+-----+-----+--------------+------------+
; |           Base 23:0            |           Limit 15:0               |
; +--------------------------------+-----------------------------------+
;  31                             16 15                                0
;
; Base  : 段基地址
; Limit : 段界限（段内最大偏移 = 段大小 - 1）
;
; ==================== Access Byte ========================
;  7   6 5  4   3 2 1 0
; +---+-----+---+--------+
; | P | DPL | S |  TYPE  |
; +---+-----+---+--------+
;
; | 位  | 名称 | 含义
; |-----|------|-------------------------------
; | 7   | P    | Present，段存在位（必须=1）
; | 6-5 | DPL  | Descriptor Privilege Level，特权级（0~3）
; | 4   | S    | Descriptor Type，1=代码/数据段，0=系统段
; | 3-0 | TYPE | 类型字段（见下）
;
; -------------------- TYPE 字段 ---------------------------
;  3   2   1   0
; +---+---+---+---+
; | E | DC| RW| A |
; +---+---+---+---+
;
; | 位 | 名称 | 含义
; |----|------|------------------------------------------
; | 3  | E    | Executable，1=代码段，0=数据段
; | 2  | DC   | Direction / Conforming（取决于段类型）：
; |    |      |   数据段(E=0)：0=向上扩展，1=向下扩展
; |    |      |   代码段(E=1)：0=非一致代码段，1=一致代码段
; | 1  | RW   | Read/Write：
; |    |      |   数据段：1=可写
; |    |      |   代码段：1=可读
; | 0  | A    | Accessed，CPU置位，表示被访问过
;
; ===================== 标志位（Flags） ====================
; | 位  | 名称 | 含义
; |-----|------|-------------------------------
; | 55  | G    | Granularity，粒度：
; |     |      |   0=字节，1=4KB
; | 54  | D/B  | Default/Big：
; |     |      |   1=32位段（代码段=32位指令，数据段=32位栈）
; | 53  | L    | Long mode，64位代码段（仅x86_64使用）
; | 52  | AVL  | Available，保留给软件使用


DESC_G_4K equ 1_00000000000000000000000b     ;G位，表示粒度
DESC_D_32 equ  1_0000000000000000000000b     ;D位，表示为32位
DESC_L    equ   0_000000000000000000000b     ;64位代码标记，此处为0即可
DESC_AVL  equ    0_00000000000000000000b     ;CPU不用此位，此位为0
DESC_LIMIT_CODE2 equ 1111_0000000000000000b   ;表示代码段的段界限值第二段
DESC_LIMIT_DATA2 equ DESC_LIMIT_CODE2         ;表示数据段的段界限值第二段
DESC_LIMIT_VIDEO2 equ 0000_0000000000000000b
DESC_P    equ 1_000000000000000b              ;表示该段存在
DESC_DPL_0 equ 00_0000000000000b              ;描述该段特权值
DESC_DPL_1 equ 01_0000000000000b
DESC_DPL_2 equ 10_0000000000000b
DESC_DPL_3 equ 11_0000000000000b
DESC_S_CODE equ 1_000000000000b               ;代码段为非系统段
DESC_S_DATA equ DESC_S_CODE                   ;数据段为非系统段
DESC_S_sys equ 0_000000000000b
DESC_TYPE_CODE equ 1000_00000000b             ;x=1,c=0,r=0,a=0,代码段可执行，非一致性，不可读，已访问位a清0

DESC_TYPE_DATA equ 0010_00000000b             ;x=0,e=0,w=1,a=0,数据段不可执行，向上扩展,可写,已访问位a清0

; ================== 定义段描述符的高四字节

DESC_CODE_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + DESC_P + DESC_S_CODE + DESC_TYPE_CODE + 0x00

DESC_DATA_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + DESC_P + DESC_S_DATA + DESC_TYPE_DATA + 0x00

DESC_VIDEO_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x0b 

; ================= 选择子属性 
RPL0 equ 00b
RPL1 equ 01b
RPL2 equ 10b
RPL3 equ 11b
TI_GDT equ 000b
TI_LDT equ 100b

; ============== 页表相关属性
PG_P equ 1b
PG_RW_R equ 00b
PG_RW_W equ 10b
PG_US_S equ 000b
PG_US_U equ 100b 

; ============== ELF 文件头相关属性
PT_NULL equ 0x0