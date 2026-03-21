%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR
    ; 输出一段自定义的提示词
    mov byte [gs:0x00],'L'
    mov byte [gs:0x01],0x0F ; 0 表示黑色背景，F 表示前景色为白色

    mov byte [gs:0x02],'o'
    mov byte [gs:0x03],0x0F

    mov byte [gs:0x04],'a'
    mov byte [gs:0x05],0x0F

    mov byte [gs:0x06],'d'
    mov byte [gs:0x07],0x0F

    mov byte [gs:0x08],'e'
    mov byte [gs:0x09],0x0F 

    mov byte [gs:0x0a],'r'
    mov byte [gs:0x0b],0x0F

    jmp $
; dd if=./loader.bin of=./hd60M.img bs=512 count=1 seek=2 conv=notrunc