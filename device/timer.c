#include "timer.h"
#include "debug.h"
#include "kernel/io.h"
#include "kernel/print.h"
#include "thread.h"
#include "interrupt.h"

#define IRQ0_FREQUENCY 100                      //咱们所期待的频率
#define INPUT_FREQUENCY 1193180                 //计数器平均CLK频率
#define COUNTRE0_VALUE  (INPUT_FREQUENCY / IRQ0_FREQUENCY)    //计数器初值
#define CONTRER0_PORT   0x40                    //计数器0的端口号
#define COUNTER0_NO     0                       //计数器0
#define COUNTER_MODE    2                       //方式2
#define READ_WRITE_LATCH 3                      //高低均写
#define PIT_CONTROL_PORT 0x43                   //控制字端口号
#define mil_seconds_per_intr (1000/IRQ0_FREQUENCY)  //每次时钟中断的毫秒数

uint32_t ticks;         //ticks是内核自开中断开启以来总共的滴答数

// 把操作的计数器counter_no,读写锁属性rwl,计数器模式counter_mode
// 写入模式控制寄存器并赋予初值 counter_value
static void _set_freq(uint8_t counter_port, uint8_t counter_no, uint8_t rwl, uint8_t counter_mode, uint16_t counter_value)
{
    // 往控制字寄存器端口0x43写入控制字
    outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1)); //计数器0,rwl高低都写，方式2,二进制表示
    // 先写入counter_value的低8位
    outb(counter_port, (uint8_t)counter_value);
    // 再写入counter_value的高8位
    outb(counter_port, (uint8_t)(counter_value >> 8));
}

// 时钟中断处理函数
static void _timer_intr_handler(void)
{
    struct _task_struct* cur_thread = running_thread();
    ASSERT(cur_thread->canary == (uint32_t)0xdeadbeef);    //检查栈是否溢出
    cur_thread->elapsed_ticks++;          //记录此线程占用的CPU时间
    ticks++;                              //从内核开启时间中断后开始至今的tick数，内核态和用户态总共的tick数
    if(cur_thread->ticks == 0)
    {   //查看时间片是否用完
        task_schedule();        // 调度
    }else{
        cur_thread->ticks--;
    }
}

// 以tick为单位的sleep，任何时间形式的sleep都会转换此形式
static void sleep_ticks(uint32_t ticks_to_sleep)
{
    uint32_t start_tick = ticks;
    // 若间隔的ticks数不够便让出CPU
    while(ticks - start_tick < ticks_to_sleep)
    {
        thread_yield();
    }
}

// 以毫秒为单位的sleep
void sleep_ms(uint32_t m_seconds)
{
    uint32_t ticks_to_sleep = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
    ASSERT(ticks_to_sleep > 0);
    sleep_ticks(ticks_to_sleep);
}

// 初始化PIT8253
void _init_timer(void)
{
    print("timer init start\n");
    // 设置8253的定时周期，也就是发中断的周期
    _set_freq(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTRE0_VALUE);
    _register_intr_handler(0x20, _timer_intr_handler);
    print("timer init done\n");
}
