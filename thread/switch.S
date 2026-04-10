[bits 32]
section .text
global switch_task
switch_task:
    ;栈中此处是返回地址
    ; 注意这里入栈要和 _thread_stack 中顺序一致
    push ebp
    mov ebp, esp
    push esi
    push edi
    push ebx
    mov eax, [ebp + 8]   ;得到栈中的参数cur
    mov [eax], esp          ;保存栈顶指针esp,task_struct的self_kstack字段
                            ;self_kstack在task_struct中的偏移为0
                            ;所以直接往thread开头处存4字节即可
    ; =========== 以上是备份当前线程的环境，下面是恢复下一个线程的环境
    mov eax, [ebp + 12]   ;栈中的参数next
    mov esp, [eax]        ;恢复下一个线程的栈顶
    pop ebx               ; 从next线程栈中恢复其寄存器, 下同
    pop edi
    pop esi
    pop ebp
    ret                   ;返回到next线程上次压入的返回地址
    ;如果此时的 next 线程之前尚未执行过，第一次执行，此时栈顶的值是初始化时设置的 kernel_thread 函数来调用指定定函数
