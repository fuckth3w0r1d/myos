ENTRY(main)

/* 定义程序头（控制 LOAD 段） */
PHDRS
{
    kernel PT_LOAD FLAGS(5);   /* R + X = 0b101 */
}

SECTIONS
{
    /* 内核虚拟地址起点（高半内核） */
    . = 0xc0001500;

    /* 代码段 + 只读数据（合并，减少 segment） */
    .text : ALIGN(4K)
    {
        *(.text*)
        *(.rodata*)
    } :kernel

    /* 数据段 + BSS（合并） */
    .data : ALIGN(4K)
    {
        *(.data*)
        *(.bss*)
        *(COMMON)
    } :kernel

    /* 调试信息（不加载到内存，但保留在 ELF） */
    .debug_info    0 : { *(.debug_info*) }
    .debug_abbrev  0 : { *(.debug_abbrev*) }
    .debug_line    0 : { *(.debug_line*) }
    .debug_str     0 : { *(.debug_str*) }
    .debug_loc     0 : { *(.debug_loc*) }
    .debug_ranges  0 : { *(.debug_ranges*) }

    /* 丢弃所有不需要的内容 */
    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}