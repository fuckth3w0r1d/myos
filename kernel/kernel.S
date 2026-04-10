[bits 32]
%define ERROR_CODE nop     ; 若在相关的异常中CPU已经自动压入了错误码，这里不做操作
%define ZERO push 0        ; 定义默认的压入错误码操作

extern IntrHandlerTable

section .data

global _intr_entry_table
_intr_entry_table:
    %macro VECTOR 2             ;这里定义一个多行宏作为 intr_handler，后面的2是指传递两个参数,里面的%1,%2代表参数
    section .text
    intr%1entry:
    ; ============= 默认压入错误码或者空操作，保证最后都会需要跨过一个4字节来进行iret
    %2
    ; ============= 保存上下文
    push ds
    push es
    push fs
    push gs
    pushad                    ;压入通用寄存器
    ; ============= 发送中断结束信号
    mov al, 0x20              ; 中断结束命令EOI，这里R为0,SL为0,EOI为1
    mov dx, 0xa0              ; 中断控制器从片端口
    out dx, al                ; 向从片发送
    mov dx, 0x20              ; 中断控制器主片端口
    out dx, al                ; 向主片发送
    ; ============= 压入中断向量号并调用中断处理函数
    push %1
    call [IntrHandlerTable + %1*4]
    jmp intr_exit
    ; ============= intr_entry_table数组(编译后会将相同的section合并成一个段)
    section .data
    dd intr%1entry             ;存储各个中断入口程序的地址,
    %endmacro                  ;多行宏结束标志


section .text
global intr_exit
intr_exit:
    ; =========== 恢复上下文
    add esp, 4                 ; 跳过中断号
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4                ; 跳过error_code
    ; =========== 返回
    iretd


VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO
VECTOR 0x08,ZERO
VECTOR 0x09,ZERO
VECTOR 0x0a,ZERO
VECTOR 0x0b,ZERO
VECTOR 0x0c,ZERO
VECTOR 0x0d,ZERO
VECTOR 0x0e,ZERO
VECTOR 0x0f,ZERO
VECTOR 0x10,ZERO
VECTOR 0x11,ZERO
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO
VECTOR 0x18,ZERO
VECTOR 0x19,ZERO
VECTOR 0x1a,ZERO
VECTOR 0x1b,ZERO
VECTOR 0x1c,ZERO
VECTOR 0x1d,ZERO
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO
VECTOR 0x20,ZERO            ;时钟中断
VECTOR 0x21,ZERO            ;键盘中断
VECTOR 0x22,ZERO            ;级联从片
VECTOR 0x23,ZERO            ;串口2对应入口
VECTOR 0x24,ZERO            ;串口1对应入口
VECTOR 0x25,ZERO            ;并口2对应入口
VECTOR 0x26,ZERO            ;软盘对应入口
VECTOR 0x27,ZERO            ;并口1对应入口
VECTOR 0x28,ZERO            ;实时时钟中断对应入口
VECTOR 0x29,ZERO            ;重定向
VECTOR 0x2a,ZERO            ;保留
VECTOR 0x2b,ZERO            ;保留
VECTOR 0x2c,ZERO            ;ps/2鼠标
VECTOR 0x2d,ZERO            ;fpu浮点单元异常
VECTOR 0x2e,ZERO            ;硬盘
VECTOR 0x2f,ZERO            ;保留

; ============= 0x80号中断，系统调用
[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
    ; ========= 保存上下文环境, 和中断栈格式一致
    push 0
    push ds
    push es
    push fs
    push gs
    pushad
    push 0x80

    ; ======== 为系统调用子功能传入参数
    push edx  ; ARG_3
    push ecx  ; ARG_2
    push ebx  ; ARG_1

    ; ======== 调用对应系统调用
    call [syscall_table + eax*4]
    add esp, 12   ;跨过上面的三个参数

    ; ========= 将call调用后的返回值存入当前内核栈中的eax的位置
    mov [esp + 8*4], eax
    jmp intr_exit     ;恢复上下文
