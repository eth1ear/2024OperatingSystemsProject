#include "ulib.h"
#include "stdio.h"
#include "string.h"
#include <stdarg.h>
#include <assert.h>

#define ZEROPAD 1 //用 0 填充,比如 %8d 在位数不足时填充前导0
#define SIGN 2
#define PLUS 4 // 强制显示加号
#define SPACE 8
#define LEFT 16
#define SPECIAL 32 

#define is_digit(c) ((c) >= '0' && (c) <= '9')

static void putstr(char * str) { 
  while(*str != '\0') {
    kputc(*str);
    str++;
  }
}

size_t my_strlen(const char *s) {
  const char *sc;
  for (sc = s; *sc != '\0'; ++sc);
  return sc - s;
}

static int get_wid(const char **s){
  int i = 0;
  while(is_digit(**s)) i = i*10 + *((*s)++) - '0';
  return i;
}

static char *number_to_string(char *str, long num, int base, int size, int type){
     char *dig = "0123456789abcdefghijklmnopqrstuvwxyz";
     char c, sign, temp[70];
     int i = 0;
     c = (type & ZEROPAD) ? '0' : ' ';//是否补充前导 0 
     sign = 0;

     if(type & SIGN){
      if(num < 0){
        num = -num;
        sign = '-';
        size--;
      }else if(type & PLUS){
        sign = '+';
        size--;
      }else if(type & SPACE){
        sign = ' ';
        size--;
      }
     }

    if(num == 0){
      temp[i++] = '0';
    }else {
      while(num != 0){
        temp[i++] = dig[ ((unsigned long)num) % (unsigned) base];
        num = ((unsigned long) num) / (unsigned) base;
      }
    }

    size -= i;

    if(!(type & (ZEROPAD | LEFT))) while(size-- > 0)*str++ = ' ';

    if(sign)*str++ = sign;

    if(!(type & LEFT))while(size-- > 0)*str++ = c;//补充前导(0或者space)
    while(i-- > 0)*str++ = temp[i];
    while(size-- > 0)*str++ = ' ';

    return str;

}

int printf(const char *fmt, ...) {
  char out[2048];
  va_list va;
  va_start(va, fmt);
  int ret = vsprintf(out, fmt, va);
  va_end(va);
  putstr(out);
  return ret;
}

int vsprintf(char *buf, const char *fmt, va_list ap) {
  int len, i;//长度
  unsigned long num;
  char *str;
  char *s;
  int flags = 0;//用来指示类型
  int integer_width;//整数的长度 如%8d, 8为精度; qualifier: h(短整)或者l(长整)
  int base;//进制

  for (str = buf; *fmt; fmt++) {

    //还没有出现需要转化的常规字符
    if (*fmt != '%') {
      *str++ = *fmt;
      continue;
    }
    flags = 0;
repeat:             
    fmt++;//这一步跳过了%,直接判断类型

    //第一个分支格式的判断
    switch (*fmt) {
      case '-':flags |= LEFT; goto repeat;
      case '+':flags |= PLUS; goto repeat;
      case ' ':flags |= SPACE; goto repeat;
      case '0':flags |= ZEROPAD; goto repeat;//补充前导 0 
    }

    //获得整数的精度
    integer_width = -1;
    if(is_digit(*fmt)){
      integer_width = get_wid(&fmt);
    }

    base = 10;// 默认基
    switch(*fmt){

      // 指针
      case 'p':
        if (integer_width == -1) {
          integer_width = 2 * sizeof(void *);
          flags |= ZEROPAD;
        }
        *str++ = '0', *str++ = 'x';
        str = number_to_string(str, (unsigned long) va_arg(ap, void *), 16, integer_width, flags);
        continue;
      // 单字符
      case 'c':
        *str++ = (unsigned char) va_arg(ap, int);
        continue;//跳到最外层的for循环
      // 字符串
      case 's':
        s = va_arg(ap, char *);
        if (!s) s = "<NULL>";
        len = my_strlen(s);
        for (i = 0; i < len; ++i) *str++ = *s++;
        continue;//跳到最外层的for循环

      case 'o': base = 8; break;//break之后进入num的获取
      case 'x': base = 16; break;
      case 'd': flags |= SIGN;
      case 'u': break;

      default:
        // assert(0);
        exit(1);
        if (*fmt != '%') *str++ = '%';
        if (*fmt) {
          *str++ = *fmt;
        } else {
          --fmt;
        }
        continue;
    }

    //如果是整型,分两种情况:带不带符号
   if(flags & SIGN){
      num = va_arg(ap, int);
    }
    else{
      num = va_arg(ap, unsigned int);
    }

    //将数字转化为字符
    str = number_to_string(str, num, base, integer_width, flags);
  }
  *str = '\0';
  return str - buf;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  int ret = vsprintf(out, fmt, va);
  va_end(va);
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  char temp[128]; // Note 128 maybe to small
  va_list va;
  va_start(va, fmt);
  vsprintf(temp, fmt, va);
  va_end(va);
  strncpy(out, temp, n);
  // snprintf函数的返回值：成功时返回源串的长度(strlen, 不含'\0')，失败时返回负值。
  return strlen(temp);
}


