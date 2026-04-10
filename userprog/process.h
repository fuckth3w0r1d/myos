#ifndef __PROCESS_H
#define __PROCESS_H
#include "thread.h"
#include "stdint.h"
#define USER_STACK3_VADDR (0xc0000000 - 0x1000)
#define USER_VADDR_START 0x804800
#define default_prio 31
void process_init(void* filename_);
void activate_page_dir(struct _task_struct* p_thread);
uint32_t* create_page_dir(void);
void update_pt_tss(struct _task_struct* p_thread);
void create_user_vaddr_bitmap(struct _task_struct* user_prog);
void process_execute(void* filename, char* name);
#endif
