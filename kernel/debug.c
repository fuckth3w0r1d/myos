#include "debug.h"
#include "kernel/print.h"
#include "interrupt.h"

// 打印文件名、行号、函数名、条件并使程序悬停
void _panic(char* filename, int line, const char* func, const char* condition)
{
    _disable_intr();                   //这里先关中断，免得别的中断打扰
    print("\n\n\n!!! error !!!\n");
    print("filename:");
    print(filename);
    print("\n");
    print("line:");
    print(line);
    print("\n");
    print("function:");
    print((char*)func);
    print("\n");
    print("condition:");
    print((char*)condition);
    print("\n");
    while(1);
}
