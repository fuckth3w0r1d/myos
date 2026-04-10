#include "sync.h"
#include "stdint.h"
#include "debug.h"
#include "global.h"
#include "interrupt.h"

// 初始化信号量
void sema_init(struct semaphore* psema, uint8_t value)
{
    psema->value = value;     //信号量赋予初值
    list_init(&psema->waiters);   //初始化信号量的等待队列
}

// 初始化锁plock
void lock_init(struct lock* plock)
{
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore, 1);
}

// 信号量down操作
void sema_down(struct semaphore* psema)
{
    // 关中断来保证原子操作
    _intr_status old_status = _disable_intr();
    while(psema->value == 0)
    {   //使用while是因为被唤醒后仍需要继续竞争条件，而不是直接向下执行
        ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));    //当前线程不应该已在等待队列当中
        if(elem_find(&psema->waiters, &running_thread()->general_tag))
        {
            PANIC("sema_down: thread blocked has been in waiters_list\n");
        }
        // 若信号量的值等于0,则将自己加入该锁的等待队列中
        list_append(&psema->waiters, &running_thread()->general_tag);
        // 阻塞自己，直到被唤醒
        thread_block(TASK_BLOCKED); //这里会调度别的线程，如果是新线程调用thread_start会开中断，如果是之前执行过的线程则会恢复到其中断状态，所以不用担心中断被关闭
    }
    // 若value为1或被唤醒之后，会执行下面代码，也就是获得了锁
    psema->value--;
    ASSERT(psema->value == 0);
    // 恢复之前的中断状态
    _set_intr_status(old_status);
}

// 信号量的up操作
void sema_up(struct semaphore* psema)
{
    // 关中断来保证原子操作
    _intr_status old_status = _disable_intr();
    ASSERT(psema->value == 0);
    if(!list_empty(&psema->waiters))
    {
        struct _task_struct* thread_blocked = elem2entry(struct _task_struct, general_tag, list_pop(&psema->waiters));
        thread_unblock(thread_blocked);
    }
    psema->value++;
    ASSERT(psema->value == 1);
    // 恢复之前的状态
    _set_intr_status(old_status);
}

// 获取锁plock
void lock_acquire(struct lock* plock)
{
    // 排除曾经自己已经持有锁但还未将其释放的状态
    if(plock->holder != running_thread())
    {
        sema_down(&plock->semaphore);   //对信号量P操作
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;
    }else{ // 线程多次申请锁，仅增加计数防止死锁
        plock->holder_repeat_nr++;
    }
}

// 释放锁plock
void lock_release(struct lock* plock)
{
    ASSERT(plock->holder == running_thread());
    if(plock->holder_repeat_nr > 1)
    {  // 释放也必须对称释放所有申请过的锁
        plock->holder_repeat_nr--;
        return;
    }
    ASSERT(plock->holder_repeat_nr == 1);
    plock->holder = NULL;     //把锁的持有者置空放在V操作之前
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);   //对信号量V操作
}
