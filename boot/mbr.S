%include "boot.inc"
; mbr 主引导程序
SECTION MBR vstart=0x7c00
    mov ax, cs      
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov sp, 0x7c00
    mov ax, 0xb800
    mov gs, ax

; 清屏利用 0x06 号功能,上卷全部行,则可清屏｡
; -----------------------------------------------------------
;INT 0x10 功能号:0x06 功能描述:上卷窗口
;------------------------------------------------------
;输入:
;AH 功能号= 0x06
;AL = 上卷的行数(如果为 0,表示全部)
;BH = 上卷行属性
;(CL,CH) = 窗口左上角的(X,Y)位置
;(DL,DH) = 窗口右下角的(X,Y)位置
;无返回值:
    mov ax, 0x600
    mov bx, 0x700
    mov cx, 0 ; 左上角: (0, 0)
    mov dx, 0x184f ; 右下角: (80,25),
; VGA 文本模式中,一行只能容纳 80 个字符,共 25 行｡
; 下标从 0 开始,所以 0x18=24,0x4f=79
    int 0x10 ; int 0x10

; 输出一段自定义的提示词
    mov byte [gs:0x00],'M'
    mov byte [gs:0x01],0x0F ; 0 表示黑色背景，F 表示前景色为白色

    mov byte [gs:0x02],'B'
    mov byte [gs:0x03],0x0F

    mov byte [gs:0x04],'R'
    mov byte [gs:0x05],0x0F

; 准备从硬盘读入loader
    mov eax, LOADER_START_SECTOR   ;起始扇区LBA地址
    mov bx, LOADER_BASE_ADDR       ;写入的内存地址
    mov cx, 4                      ;待读入的扇区数 这里读入4个扇区 (loader比较大)
    call rd_disk_m_16             ;以下读取程序的起始部分（一个扇区）
    jmp LOADER_BASE_ADDR

; | 端口      | 作用        
; | 0x1F0    | 数据端口      
; | 0x1F1    | 错误        
; | 0x1F2    | 扇区数       
; | 0x1F3    | LBA低8位    
; | 0x1F4    | LBA中8位   
; | 0x1F5    | LBA高8位    
; | 0x1F6    | 设备+LBA高4位 
; | 0x1F7    | 命令 / 状态   


rd_disk_m_16:
    ; ======== 保存参数
    mov esi, eax
    mov di, cx
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
    ; ========== 检查硬盘状态
; | 位 | 名称                | 含义    | 常用判断        
; | 7 | BSY (Busy)          | 硬盘忙   | `1=忙，不能操作`  
; | 6 | DRDY (Ready)        | 设备就绪 | `1=可以接受命令`  
; | 5 | DF (Device Fault)   | 设备错误 | `1=硬件故障`    
; | 4 | DSC (Seek Complete) | 寻道完成 | 很少用         
; | 3 | DRQ (Data Request)  | 数据请求 | `1=可以读/写数据` 
; | 2 | CORR (Corrected)    | 数据纠错 | 一般忽略        
; | 1 | IDX (Index)         | 索引     | 基本不用        
; | 0 | ERR (Error)         | 错误发生 | `1=命令失败`    
    mov cx, di          ; 外层循环：扇区数
.read_sector:
    push cx ; 保存外层循环索引 cx
.disk_not_ready:
    nop
    mov dx, 0x1f7
    in al, dx
    and al, 0x88  ; 10001000
    cmp al, 0x08  ; 00001000 硬盘不忙且可以读写数据
    jnz .disk_not_ready  ; 等待当前扇区就绪
    mov cx, 0x100          ; 内层：读256个字 = 512字节, 读一个扇区
    mov dx, 0x1f0
.continue_read:
    in ax, dx
    mov [bx], ax
    add bx, 2
    loop .continue_read
    pop cx
    loop .read_sector
    ret

    times 510-($-$$) db 0 ;填充到510字节
    db 0x55, 0xaa ; magic number 用于检测是否为可启动扇区

;dd if=./mbr.bin of=./hd60M.img bs=512 count=1 conv=notrunc