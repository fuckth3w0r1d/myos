#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"

// 初始化io队列ioq
void ioqueue_init(struct ioqueue* ioq)
{
    lock_init(&ioq->lock);     // 初始化io队列的锁
    ioq->producer = ioq->consumer = NULL;  // 生产者和消费者置空
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
static void ioq_wait(struct _task_struct** waiter)
{
    ASSERT(waiter != NULL && *waiter == NULL);
    *waiter = running_thread();
    thread_block(TASK_BLOCKED);
}

// 唤醒waiter
static void wakeup(struct _task_struct** waiter)
{
    ASSERT(*waiter != NULL);
    thread_unblock(*waiter);
    *waiter = NULL;
}

// 消费者从ioq队列中获取一个字符
char ioq_getchar(struct ioqueue* ioq)
{
    ASSERT(_get_intr_status() == INTR_OFF);

    lock_acquire(&ioq->lock);
    // 若缓冲区(队列)为空,把消费者ioq->consumer记为当前线程自己等待缓冲区
    while(ioq_empty(ioq))
    {
        ioq_wait(&ioq->consumer);
    }
    char byte = ioq->buf[ioq->head];	  // 从缓冲区中取出
    ioq->head = next_pos(ioq->head);	  // 把读游标移到下一位置
    if(ioq->producer != NULL) wakeup(&ioq->producer);		  // 唤醒生产者
    lock_release(&ioq->lock);
    return byte;
}

// 生产者往ioq队列中写入一个字符byte
void ioq_putchar(struct ioqueue* ioq, char byte)
{
    ASSERT(_get_intr_status() == INTR_OFF);
    lock_acquire(&ioq->lock);
    // 若缓冲区(队列)已经满了,把生产者ioq->producer记为自己等待缓冲区
    while(ioq_full(ioq))
    {
        ioq_wait(&ioq->producer);
    }
    ioq->buf[ioq->tail] = byte;      // 把字节放入缓冲区中
    ioq->tail = next_pos(ioq->tail); // 把写游标移到下一位置
    if(ioq->consumer != NULL) wakeup(&ioq->consumer);          // 唤醒消费者
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
