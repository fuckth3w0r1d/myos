TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

; |  Index   | 名称                     | 访问|
; | -------- | ------------------------- | --- |
; | `00h`    | Horizontal Total          | R/W |
; | `01h`    | End Horizontal Display    | R/W |
; | `02h`    | Start Horizontal Blanking | R/W |
; | `03h`    | End Horizontal Blanking   | R/W |
; | `04h`    | Start Horizontal Retrace  | R/W |
; | `05h`    | End Horizontal Retrace    | R/W |
; | `06h`    | Vertical Total            | R/W |
; | `07h`    | Overflow Register         | R/W |
; | `08h`    | Preset Row Scan           | R/W |
; | `09h`    | Maximum Scan Line         | R/W |
; | `0Ah`    | Cursor Start Register     | R/W |
; | `0Bh`    | Cursor End Register       | R/W |
; | `0Ch`    | Start Address High        | R/W |
; | `0Dh`    | Start Address Low         | R/W |
; | `0Eh`    | Cursor Location High      | R/W |
; | `0Fh`    | Cursor Location Low       | R/W |
; | `10h`    | Vertical Retrace Start    | R/W |
; | `11h`    | Vertical Retrace End      | R/W |
; | `12h`    | Vertical Display End      | R/W |
; | `13h`    | Offset Register           | R/W |
; | `14h`    | Underline Location        | R/W |
; | `15h`    | Start Vertical Blanking   | R/W |
; | `16h`    | End Vertical Blanking     | R/W |
; | `17h`    | CRTC Mode Control         | R/W |
; | `18h`    | Line Compare Register     | R/W |

; | 功能                                                    | I/O 端口（读/写）
; | **CRT Controller Index Register** — 选择要访问的子寄存器  | `0x3B4` / `0x3D4`（取决于显示模式）
; | **CRT Controller Data Register** — 读写所选子寄存器的数据 | `0x3B5` / `0x3D5`（取决于显示模式）

; index reg 设置后，对 data reg 的读写就是对 index reg 指定的寄存器的读写

; 屏幕 80*25 字节, 一个字符两字节, 光标值是字符索引

[bits 32]

section .data
put_int_buffer dq 0  ; 8 字节缓冲区
section .text
; ================= 打印单字符函数
global _put_char
_put_char:
    push ebp
    mov ebp, esp
    pushad  ; 将 32 位通用寄存器全部压栈 后续使用 popad 恢复
    mov ax, SELECTOR_VIDEO
    mov gs, ax  ; 设置好 gs 以防范万一
    ; ================= 获取光标位置
    mov dx, 0x3d4
    mov al, 0xe
    out dx, al
    mov dx, 0x3d5
    in al, dx
    mov ah, al ;获取高8位
    mov dx, 0x3d4
    mov al, 0xf
    out dx, al
    mov dx, 0x3d5
    in al, dx ;获取低8位
    mov bx, ax ; 存入 bx

    mov cl, [ebp + 8] ;跳过ebp和返回地址获取参数
    cmp cl, 0xd
    jz .is_CR ; 回车
    cmp cl, 0xa
    jz .is_LF ; 换行
    cmp cl, 0x8
    jz .is_backspace ; 退格
    jmp .put_other

    ; ==================== 回车
    .is_CR:
    xor dx, dx        ;dx是被除数的高16位，清0
    mov ax, bx        ;ax 是被除数的低16位
    mov si, 80
    div si            ;光标值减去除以80的余数便是取整，div指令中dx存放余数
    sub bx, dx        ;光标移动到首列
    jmp .set_cursor

    ; ===================== 退格
    .is_backspace:
    cmp bx, 0
    jz .set_cursor    ; 如果光标值为0则不处理
    dec bx            ;光标值是字符索引, 先把光标值减一
    shl bx,1          ;一个字符占2字节，乘2得到光标在显存的偏移
    mov byte [gs:bx], 0x20    ;将待删除字符低字节传入0x20, 指空格
    inc bx
    mov byte [gs:bx], 0x7     ;高字节传入0x7 表示黑屏白字
    shr bx, 1         ;恢复光标值
    jmp .set_cursor
    ; ===================== 其他正常字符
    .put_other:
    shl bx,1          ;表示对应显存中的偏移字节
    mov [gs:bx], cl   ;ASCII字符本身
    inc bx
    mov byte[gs:bx], 0x07     ;字符属性
    shr bx, 1                 ;下一个光标值
    inc bx
    cmp bx, 2000
    jl .set_cursor            ;若小于2000,则表示没写道显存最后，因为80×25=2000
                              ; 否则继续往下执行换行和滚屏
    ; ===================== 换行 (我设置为\n同时执行\r和狭义的\n)
    .is_LF:
    xor dx, dx        ;dx是被除数的高16位，清0
    mov ax, bx        ;ax 是被除数的低16位
    mov si, 80
    div si            ;光标值减去除以80的余数便是取整，div指令中dx存放余数
    sub bx, dx        ;光标移动到首列
    add bx, 80        ;换行加上80
    cmp bx, 2000
    jl .set_cursor    ; 大于2000则往下执行滚屏

    ;====================== 滚屏
    ;将1～24行搬运到0～23,然后24行填充空格
    .roll_screen:
    cld               ;设置方向标志位为增
    mov ecx, 960      ;2000-80=1920,共1920×2=3840个字符，而movsd一次复制4字节，所以需要3840/4=960次
    mov esi, 0xc00b80a0   ;第1行行首
    mov edi, 0xc00b8000   ;第0行行首
    rep movsd         ;循环复制
    ; ==== 将最后一行填充为空白
    mov ebx, 3840     ;最后一行首字符的地一个字节偏移 = 1920×2
    mov ecx, 80
    .cls:
    mov word [gs:ebx], 0x0720     ;0x0720是黑底白字空格
    add ebx, 2
    loop .cls
    mov bx, 1920      ; 移动光标至最后一行开头
                      ; 滚屏完成默认往下写回光标
    ; ================== 写回光标值
    .set_cursor:
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    mov al, bh
    out dx, al ; 写回高八位

    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al ; 写回低八位

    .put_char_done:
    popad ; 恢复通用寄存器
    leave
    ret

; =================== 打印字符串函数
global _put_str
_put_str:
    push ebp
    mov ebp, esp
    push ebx           ;此函数只用到了ebx和ecx
    push ecx
    xor ecx, ecx
    mov ebx, [ebp + 8]       ;获取打印字符串地址
    .go_on:
    mov cl, [ebx]
    cmp cl, 0                 ;若这里为0则说明到了字符串末尾
    jz .str_over
    push ecx                  ;为put_char函数传递参数
    call _put_char
    add esp, 4                ;清理栈中参数
    inc ebx
    jmp .go_on

    .str_over:
    pop ecx
    pop ebx
    leave
    ret

; =================== 打印无符号整数函数
global _put_uint
_put_uint:
    push ebp
    mov ebp, esp
    pushad
    mov eax, [ebp + 8] ; 获取参数
    mov edx, eax      ; 保存参数
    mov edi, 7 ; buffer 初始偏移量, 从最后一个开始写
    mov ecx, 8        ;32位数据中16位数据占8位
    mov ebx, put_int_buffer
    ; ======= 转 ascii
    .hex_to_ascii:     ;每4位二进制是十六进制的1位
    and edx, 0x0000000f   ;解析十六进制的每一位
    cmp edx, 9        ;数字0～9和a~f分别处理成 ascii 码
    jg .above_9
    add edx, '0'
    jmp .store
    .above_9:
    sub edx, 10
    add edx, 'A'

    .store:
    mov [ebx+edi], dl ;此时dl中应该是数字对应的ASCII码
    dec edi
    shr eax, 4
    mov edx, eax      ; 更新一下edx中待处理参数
    loop .hex_to_ascii

    ; ======= 去除前导 0
    .ready_to_print:
    inc edi           ;此时edi为-1（0xFFFFFFFF），加一变为0
    .skip_prefix_0:
    cmp edi, 8        ;若已经比较到第9个字符了则表示待打印字符串全为0
    je .full0
    ; 去除前导0后edi作为非0的最高位字符的偏移
    .go_on_skip:
    mov cl, [put_int_buffer + edi]
    inc edi
    cmp cl, '0'
    je .skip_prefix_0 ;继续判断下一个字符
    dec edi           ;由于edi指向的是下一个字符，所以这里得减一是的edi指向当前不为0字符
    ; ======== 输出 0x 前缀
    push dword '0'
    call _put_char
    add esp, 4
    push dword 'x'
    call _put_char
    add esp, 4
    jmp .put_each_num

    ; ========== 输出数字
    .full0:
    mov cl, '0'       ;输入全为0,则只打因0
    .put_each_num:
    push ecx          ;此时cl中为可打印字符
    call _put_char
    add esp, 4        ;清理栈中参数
    inc edi
    mov cl, [put_int_buffer + edi]
    cmp edi, 8
    jl .put_each_num
    popad
    leave
    ret

; ======================= 打印有符号整数函数
global _put_int
_put_int:
    push ebp
    mov ebp, esp
    pushad
    mov eax, [ebp + 8] ; 获取参数
    cmp eax, 0
    jge .positive
    ; ===== 负数处理
    mov cl, '-'          ; 先输出负号
    push ecx
    call _put_char
    add esp, 4
    not eax
    inc eax              ; 再取绝对值（按位取反再加一）
    ; ====== 统一走无符号输出
    .positive:
    push eax
    call _put_uint
    add esp, 4
    popad
    leave
    ret


global set_cursor
set_cursor:
    push ebp
    mov ebp, esp
    pushad
    mov bx, [ebp+8]
    ; ===== 先设置高8位
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    mov al, bh
    out dx, al

    ; ===== 再设置低8位
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al
    popad
    leave
    ret
