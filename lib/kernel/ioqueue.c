#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"

// 初始化io队列ioq
void ioqueue_init(struct ioqueue* ioq)
{
    lock_init(&ioq->lock);     // 初始化io队列的锁
    list_init(&ioq->producers);
    list_init(&ioq->consumers);  // 生产者队列和消费者队列初始化
    ioq->head = ioq->tail = 0; // 队列的首尾指针指向缓冲区数组第0个位置
}

// 返回pos在缓冲区中的下一个位置值
static int32_t next_pos(int32_t pos)
{
    return (pos + 1) % bufsize;
}

// 判断队列是否已满
bool ioq_full(struct ioqueue* ioq)
{
    ASSERT(_get_intr_status() == INTR_OFF);
    return next_pos(ioq->tail) == ioq->head;
}

// 判断队列是否已空
bool ioq_empty(struct ioqueue* ioq)
{
    ASSERT(_get_intr_status() == INTR_OFF);
    return ioq->head == ioq->tail;
}

// 使当前生产者或消费者在此缓冲区上等待
static void ioq_wait(struct list* waiters, struct lock* lock)
{
    struct _task_struct* cur = running_thread();
    ASSERT(!elem_find(waiters, &cur->general_tag));
    list_append(waiters, &cur->general_tag);
    lock_release(lock);
    thread_block(TASK_BLOCKED);
    lock_acquire(lock);
}

// 唤醒waiter
static void ioq_wakeup(struct list* waiters)
{
    if(!list_empty(waiters))
    {
        struct _task_struct* thread_blocked = elem2entry(struct _task_struct, general_tag, list_pop(waiters));
        thread_unblock(thread_blocked);
    }
}

// 消费者从ioq队列中获取一个字符
char ioq_getchar(struct ioqueue* ioq)
{
    ASSERT(_get_intr_status() == INTR_OFF);
    lock_acquire(&ioq->lock);
    // 若缓冲区(队列)为空,把消费者加入ioq->consumer等待队列
    while(ioq_empty(ioq))
    {
        ioq_wait(&ioq->consumers, &ioq->lock);
    }
    char byte = ioq->buf[ioq->head];	  // 从缓冲区中取出
    ioq->head = next_pos(ioq->head);	  // 把读游标移到下一位置
    ioq_wakeup(&ioq->producers);		  // 唤醒生产者
    lock_release(&ioq->lock);
    return byte;
}

// 生产者往ioq队列中写入一个字符byte
void ioq_putchar(struct ioqueue* ioq, char byte)
{
    ASSERT(_get_intr_status() == INTR_OFF);
    lock_acquire(&ioq->lock);
    // 若缓冲区(队列)已经满了,把生产者加入ioq->producer等待队列
    while(ioq_full(ioq))
    {
        ioq_wait(&ioq->producers, &ioq->lock);
    }
    ioq->buf[ioq->tail] = byte;      // 把字节放入缓冲区中
    ioq->tail = next_pos(ioq->tail); // 把写游标移到下一位置
    ioq_wakeup(&ioq->consumers);          // 唤醒消费者
    lock_release(&ioq->lock);
}

// 返回环形缓冲区中的数据长度
uint32_t ioq_length(struct ioqueue* ioq)
{
    uint32_t len = 0;
    if(ioq->head <= ioq->tail)
    {
        len = ioq->tail - ioq->head;
    }else{
        len = bufsize - (ioq->head - ioq->tail);
    }
    return len;
}
