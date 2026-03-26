#include "interrupt.h"
#include "global.h"
#include "stdint.h"
#include "io.h"

#define IDT_DESC_CNT 0x21           //目前总支持的中断数
#define PIC_M_CTRL 0x20             //主片控制端口
#define PIC_M_DATA 0x21             //主片数据端口
#define PIC_S_CTRL 0xA0             //从片控制端口
#define PIC_S_DATA 0xA1             //从片数据端口

// 中断描述符结构体
struct IntrDesc{
    uint16_t func_offset_low_word;    // 中断处理函数地址低16位
    uint16_t selector;                // 代码段选择子
    uint8_t zero;                    // 不用考虑，必须为0
    uint8_t attribute;               // 类型 属性
    uint16_t func_offset_high_word;  // 中断处理函数地址高16位
};

// 定义 IDT
static struct IntrDesc IDT[IDT_DESC_CNT];  //idt是中断描述符表，本质上是中断描述符数组

char* intr_name[IDT_DESC_CNT];              //用于保存中断的名字
intrHandler_ptr intrHandler_table[IDT_DESC_CNT];       //定义中断处理程序地址数组
// 引用 kernel.S 中的 intr_entry_table, 每个成员为 intrHandler_ptr 类型(头文件里自定义的void*)
extern intrHandler_ptr intr_entry_table[IDT_DESC_CNT];

// 初始化中断控制器
static void initPIC(void)
{
    // 初始化主片
    outb(PIC_M_CTRL, 0x11);           //ICW1:边沿触发，级联8259,需要ICW4
    outb(PIC_M_DATA, 0x20);           //ICW2:起始中断向量号为0x20,也就是IRQ0的中断向量号为0x20
    outb(PIC_M_DATA, 0x04);           //ICW3:设置IR2接从片
    outb(PIC_M_DATA, 0x01);           //ICW4:8086模式，正常EOI，非缓冲模式，手动结束中断

    // 初始化从片
    outb(PIC_S_CTRL, 0x11);           //ICW1:边沿触发，级联8259,需要ICW4
    outb(PIC_S_DATA, 0x28);           //ICW2：起始中断向量号为0x28
    outb(PIC_S_DATA, 0x02);           //ICW3:设置从片连接到主片的IR2引脚
    outb(PIC_S_DATA, 0x01);           //ICW4:同上

    // 打开主片上的IR0,也就是目前只接受时钟产生的中断
    outb(PIC_M_DATA, 0xfe);           //OCW1:IRQ0外全部屏蔽
    outb(PIC_S_DATA, 0xff);           //OCW1:IRQ8~15全部屏蔽

    put_str("pic init done!\n");
}

// 创建中断描述符
static void makeIntrDesc(struct IntrDesc* p_gdesc, uint8_t attr, intrHandler_ptr function)
{  //参数分别为，描述符地址，属性，中断程序入口
    p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
    p_gdesc->selector = SELECTOR_K_CODE;  // 内核代码
    p_gdesc->zero = 0;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

// 初始化中断描述符表
static void initIDT(void)
{
    for(int i = 0; i < IDT_DESC_CNT; i++)
    {
        makeIntrDesc(&IDT[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }
    put_str("init IDT done\n");
}


// 通用的中断处理函数，一般用在出现异常的时候处理
static void intrHandler_default(uint64_t vec_nr)
{
    // IRQ7和IRQ15会产生伪中断，IRQ15是从片上最后一个引脚，保留项，这俩都不需要处理
    if(vec_nr == 0x27 || vec_nr == 0x2f) return;
    put_str("int vector : 0x");       //这里仅实现一个打印中断号的功能
    put_uint(vec_nr);
    put_char('\n');
}


static void initIntrHandlerTable(void)
{
    for(int i = 0; i < IDT_DESC_CNT; i++)
    {
        // idt_table数组中的函数是在进入中断后根据中断向量号调用的
        intrHandler_table[i] = intrHandler_default;    //这里初始化为最初的默认处理函数, 后续再实现各个中断处理函数的注册
        intr_name[i] = "unknown";               //先统一赋值为unknown
    }
    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    //intr_name[15]是保留项，未使用
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}

// 完成有关中断的所有初始化工作
void initInterrupt(void)
{
    put_str("initInterrupt start\n");
    initIDT();               //初始化中断描述符表
    initIntrHandlerTable();  //初始化中断处理函数表和中断名称
    initPIC();               //初始化8259A

    // 加载 IDT
    uint64_t idt_operand = (sizeof(IDT)-1) | ((uint64_t)((uint32_t)IDT << 16));   //这里(sizeof(IDT)-1)是表示段界限，占16位，然后我们的idt地址左移16位表示高32位，表示idt首地址
    asm volatile("lidt %0" : : "m" (idt_operand));
    put_str("init interrupt all done\n");
}
