#include "print.h"
int main(void)
{
   put_char('K');
   put_char('e');
   put_char('r');
   put_char('n');
   put_char('e');
   put_char('l');
   put_char('\n');
   put_char('r');
   put_char('3');
   put_char('t');
   put_char('2');
   put_char('\n');
   put_str("kernel made by r3t2\n");
   put_uint(100);
   put_uint(4096);
   put_uint(0xffffffff);
   while(1);
   return 0;
}
