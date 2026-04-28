#ifndef __DEVICE_TIMER_H
#define __DEVICE_TIMER_H
#include "stdint.h"
void _init_timer(void);  //初始化PIT
void sleep_ms(uint32_t m_seconds);
#endif
