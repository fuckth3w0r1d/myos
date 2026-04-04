#include "interrupt.h"
#include "global.h"
#include "stdint.h"
#include "kernel/io.h"
#include "kernel/print.h"

#define IDT_DESC_CNT 0x30           //目前总支持的中断数
#define PIC_M_CTRL 0x20             //主片控制端口
#define PIC_M_DATA 0x21             //主片数据端口
#define PIC_S_CTRL 0xA0             //从片控制端口
#define PIC_S_DATA 0xA1             //从片数据端口

#define EFLAGS_IF 0x00000200        //eflags寄存器中的if位为1
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl\n\t pop %0" : "=g" (EFLAG_VAR))   //pushfl是指将eflags寄存器值压入栈顶

// 中断描述符结构体
struct _intr_desc{
    uint16_t func_offset_low_word;    // 中断处理函数地址低16位
    uint16_t selector;                // 代码段选择子
    uint8_t zero;                    // 不用考虑，必须为0
    uint8_t attribute;               // 类型 属性
    uint16_t func_offset_high_word;  // 中断处理函数地址高16位
};

// 定义 IDT
static struct _intr_desc IDT[IDT_DESC_CNT];  //idt是中断描述符表，本质上是中断描述符数组

char* IntrName[IDT_DESC_CNT];              //用于保存中断的名字
_intr_handler_ptr IntrHandlerTable[IDT_DESC_CNT];       //定义中断处理程序地址数组
// 引用 kernel.S 中的 _intr_entry_table, 每个成员为 _intr_handler_ptr 类型(头文件里自定义的void*)
extern _intr_handler_ptr _intr_entry_table[IDT_DESC_CNT];

// 初始化中断控制器
static void _init_PIC(void)
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

    // 只接受时钟产生的中断和键盘中断
    outb(PIC_M_DATA, 0xfc);           //OCW1:IRQ0和IRQ1以外全部屏蔽
    outb(PIC_S_DATA, 0xff);           //OCW1:IRQ8~15全部屏蔽

    print("pic init done!\n");
}

// 创建中断描述符
static void _make_IntrDesc(struct _intr_desc* p_gdesc, uint8_t attr, _intr_handler_ptr function)
{  //参数分别为，描述符地址，属性，中断程序入口
    p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
    p_gdesc->selector = SELECTOR_K_CODE;  // 内核代码
    p_gdesc->zero = 0;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

// 初始化中断描述符表
static void _init_IDT(void)
{
    for(int i = 0; i < IDT_DESC_CNT; i++)
    {
        _make_IntrDesc(&IDT[i], IDT_DESC_ATTR_DPL0, _intr_entry_table[i]);
    }
    print("init IDT done\n");
}


// 通用的中断处理函数，一般用在出现异常的时候处理
static void _intr_handler_default(uint32_t vec_nr)
{
    // IRQ7和IRQ15会产生伪中断，IRQ15是从片上最后一个引脚，保留项，这俩都不需要处理
    if(vec_nr == 0x27 || vec_nr == 0x2f) return;
    // 将光标置为0,从屏幕左上角清出一片打印异常信息的区域方便阅读
    set_cursor(0);    //这里是print.S中的设置光标函数，光标值范围是0～1999
    int cursor_pos = 0;
    while(cursor_pos < 320)
    {
        print(" ");
        cursor_pos++;
    }
    set_cursor(0);    //重置光标值
    print("!!!!!!!!  exception message begin  !!!!!!!!");
    set_cursor(88);    //从第二行第8个字符开始打印
    print(IntrName[vec_nr]);
    if(vec_nr == 14)
    {   //若为PageFault,将缺失的地址打印出来并悬停
        uint32_t page_fault_vaddr = 0;
        asm("movl %%cr2, %0" : "=r"(page_fault_vaddr));     //cr2存放造成PageFault的地址
        print("\npage fault addr is ");
        print(page_fault_vaddr);
    }
    print("\n!!!!!!!!   exception message end !!!!!!!");
    // 能进入中断处理程序就表示已经在关中断情况下了，不会出现调度进程的情况，因此下面的死循环不会被中断
    while(1);
}

// 在中断处理程序数组第vec_num个元素中注册安装中断处理程序function
void _register_intr_handler(uint8_t vec_num, _intr_handler_ptr function)
{
    IntrHandlerTable[vec_num] = function;
}

static void _init_intr_handler_table(void)
{
    for(int i = 0; i < IDT_DESC_CNT; i++)
    {
        // idt_table数组中的函数是在进入中断后根据中断向量号调用的
        IntrHandlerTable[i] = _intr_handler_default;    //这里初始化为最初的默认处理函数, 后续再实现各个中断处理函数的注册
        IntrName[i] = "unknown";               //先统一赋值为unknown
    }
    IntrName[0] = "#DE Divide Error";
    IntrName[1] = "#DB Debug Exception";
    IntrName[2] = "NMI Interrupt";
    IntrName[3] = "#BP Breakpoint Exception";
    IntrName[4] = "#OF Overflow Exception";
    IntrName[5] = "#BR BOUND Range Exceeded Exception";
    IntrName[6] = "#UD Invalid Opcode Exception";
    IntrName[7] = "#NM Device Not Available Exception";
    IntrName[8] = "#DF Double Fault Exception";
    IntrName[9] = "Coprocessor Segment Overrun";
    IntrName[10] = "#TS Invalid TSS Exception";
    IntrName[11] = "#NP Segment Not Present";
    IntrName[12] = "#SS Stack Fault Exception";
    IntrName[13] = "#GP General Protection Exception";
    IntrName[14] = "#PF Page-Fault Exception";
    //IntrName[15]是保留项，未使用
    IntrName[16] = "#MF x87 FPU Floating-Point Error";
    IntrName[17] = "#AC Alignment Check Exception";
    IntrName[18] = "#MC Machine-Check Exception";
    IntrName[19] = "#XF SIMD Floating-Point Exception";
}

// 开中断并返回开中断前的状态
_intr_status _enable_intr(void)
{
    _intr_status old_status;
    if (INTR_ON == _get_intr_status())
    {
        old_status = INTR_ON;
        return old_status;
    }else{
        old_status = INTR_OFF;
        asm volatile("sti");     //开中断，sti指令将IF位置为1
        return old_status;
    }
}

// 关中断并返回关中断前的状态
_intr_status _disable_intr(void)
{
    _intr_status old_status;
    if(INTR_ON == _get_intr_status())
    {
        old_status = INTR_ON;
        asm volatile("cli" : : : "memory");
        return old_status;
    }else{
        old_status = INTR_OFF;
        return old_status;
    }
}

// 将中断状态设置为 status 并返回设置前的中断状态
_intr_status _set_intr_status(_intr_status status)
{
    return (status == INTR_ON) ? _enable_intr() : _disable_intr();
}

// 获取当前中断状态
_intr_status _get_intr_status(void)
{
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF ;
}


// 完成有关中断的所有初始化工作
void _init_interrupt(void)
{
    print("init interrupt start\n");
    _init_IDT();               //初始化中断描述符表
    _init_intr_handler_table();  //初始化中断处理函数表和中断名称
    _init_PIC();               //初始化8259A

    // 加载 IDT
    uint64_t idt_operand = (sizeof(IDT)-1) | ((uint64_t)(uint32_t)IDT << 16);   //这里(sizeof(IDT)-1)是表示段界限，占16位，然后我们的idt地址左移16位表示高32位，表示idt首地址
    asm volatile("lidt %0" : : "m" (idt_operand));
    print("init interrupt all done\n");
}
