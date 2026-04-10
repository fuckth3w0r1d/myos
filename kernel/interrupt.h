#ifndef __KERNEL_INTERRUPT_H
#define __KERNEL_INTERRUPT_H
#include "stdint.h"
typedef void* _intr_handler_ptr;

typedef _Bool _intr_status; //中断状态
#define INTR_OFF 0         //关中断
#define INTR_ON 1         //开中断
_intr_status _enable_intr(void);
_intr_status _disable_intr(void);
_intr_status _set_intr_status(_intr_status status);
_intr_status _get_intr_status(void);

void _register_intr_handler(uint8_t vec_num, _intr_handler_ptr function);

void _init_interrupt(void);

#endif
