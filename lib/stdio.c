#include "stdio.h"
#include "stdint.h"
#include "global.h"
#include "string.h"
#include "stdtype.h"
#include "user/syscall.h"
#define va_start(ap, v) ap = (va_list)&v    //把ap指向第一个固定参数v
#define va_arg(ap, t) *((t*)(ap += 4))      //ap指向下一个参数并返回其值
#define va_end(ap) ap = NULL                //清除ap

// 将整型转换成字符
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base)
{
    uint32_t m = value % base;        //取余
    uint32_t i = value / base;        //取整
    if(i) itoa(i, buf_ptr_addr, base); //如果倍数部位0,则递归调用
    if(m < 10)
    {   //如果余数是0～9
        *((*buf_ptr_addr)++) = m + '0';     //将数字0～9转化为字符'0'～'9'
    }else{
        *((*buf_ptr_addr)++) = m - 10 + 'A'; //将数字A~F转化为字符'A'～'F'
    }
}

// 将参数ap按照格式format输出到字符串str，并返回替换后的str长度
uint32_t vsprintf(char* str, const char* format, va_list ap)
{
    char* dst_ptr = str;
    const char* src_ptr = format;
    char ch = *src_ptr;
    int32_t arg_int;
    char* arg_str;
    while(ch)
    {
        if(ch != '%')
        {
            *dst_ptr = ch;
            dst_ptr++;
            src_ptr++;
            ch = *src_ptr;
            continue;
        }
        src_ptr++;
        ch = *src_ptr;    //获取%后面的字符
        switch(ch){
        case 'x':             //十六进制打印
            arg_int = va_arg(ap, int);  //最开始ap是指向format，然后每次取下一个参数
            itoa(arg_int, &dst_ptr, 16);
            src_ptr++;
            ch = *src_ptr; //跳过格式字符并且更新ch
            break;
        case 's':             //字符串打印
            arg_str = va_arg(ap, char*);
            strcpy(dst_ptr, arg_str);
            dst_ptr += strlen(arg_str);
            src_ptr++;
            ch = *src_ptr; //跳过格式字符并且更新ch
            break;
        case 'c':
            *dst_ptr = va_arg(ap, char);
            dst_ptr++;
            src_ptr++;
            ch = *src_ptr; //跳过格式字符并且更新ch
            break;
        case 'd':
            arg_int = va_arg(ap, int);
            // 如果是负数，将其转换为正数后，在正数前面输出一个负号
            if(arg_int < 0)
            {
                arg_int = 0 - arg_int;
                *dst_ptr = '-';
                dst_ptr++;
            }
            itoa(arg_int, &dst_ptr, 10);
            src_ptr++;
            ch = *src_ptr; //跳过格式字符并且更新ch
            break;
        }
    }
    return strlen(str);
}

// 格式化输出字符串format
uint32_t printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char buf[1024] = {0};
    vsprintf(buf, format, args);
    va_end(args);
    return write(buf);
}
