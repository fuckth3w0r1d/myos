%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR 
; ============ 从 mbr 接管，跳转到 loader_start
jmp loader_start

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
  
; ========= 定义gdt的指针，前2字节是gdt的界限，后4字节是gdt的起始地址 ---------
gdt_ptr dw GDT_LIMIT    ;define word
        dd GDT_BASE

loader_start:
    ; ========= 准备进入保护模式 
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
    ; ====== 已经进入保护模式
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

    mov byte [gs:0x10], 'p' ; 打印提示表明程序顺利执行到这了

    jmp $

; dd if=./loader.bin of=./hd60M.img bs=512 count=4 seek=2 conv=notrunc