#include <stdint.h>
#include "string.h"

size_t strlen(const char *s) {
  size_t len = 0;
  while(s[len] != '\0')++len;
  return len;
}

char *strcpy(char *s1, const char *s2) {
  char *s = s1;

  for(s = s1; (*s++ = *s2++) != '\0';)
    ;
  return (s1);
}

char *strncpy(char *dst, const char *src, size_t n) {

  size_t i;

  for(i = 0; i < n && src[i] != '\0'; i++)
     dst[i] = src[i];
  for(; i < n; i++)
     dst[i] = '\0';

  return dst;
}

char *strcat(char *dst, const char *src) {
  char *s;

  for(s = dst; *s != '\0'; ++s)
    ;
  for(;(*s = *src) != '\0'; ++s, ++src)
    ;

  return (dst);
}

int strcmp(const char *s1, const char *s2) {
  for( ; *s1 == *s2; s1++, s2++){
    if(*s1 == '\0')return 0;
  }

  return *s1 - *s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  if(n == 0)return 0;

  while(--n && *s1 && *s1 == *s2){
    s1++;

    s2++;
  }

  return (*s1 - *s2);
}

void *memset(void *s, int c, size_t n) {
  const unsigned char uc = c; //unsigned char 占一个字节，意味着只截取 c 的后八位
  unsigned char *su;
  for(su = s; 0 < n; ++su, --n)
    *su = uc;
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  
  char *src1;
  const char *src2;

  src1 = dst;
  src2 = src;
  if(src2 < src1 && src1 < src2 + n)
    for(src1 += n, src2 += n; 0 < n; --n)
      *--src1 = *--src2;
  else
    for(; 0 < n; --n)*src1++ = *src2++;

  return (dst);
}

void *memcpy(void *out, const void *in, size_t n) {
    char *out_u;
    const char *in_u;
    for(out_u = out, in_u = in; 0 < n; ++out_u, ++in_u, --n)
      *out_u = *in_u;

    return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  
  const unsigned char *su1, *su2;
  for(su1 = s1, su2 = s2; 0 < n; ++su1, ++su2, --n)
     if(*su1 != *su2)
        return ((*su1 < *su2) ? -1 : +1);

  return (0);
}

