#ifndef _COMMON_H
#define _COMMON_H

#include <assert.h>
#include <kernel.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// 注意到我们的make test中加了-DTEST选项，所以这里的宏定义会生效
#ifdef TEST
// #define debug(...) printf(__VA_ARGS__)
// debug off
#define debug(...)
#else
#define debug(...)
#endif

#define PANIC(fmt, ...)                                                  \
  do {                                                                   \
    fprintf(stderr, "\033[1;41mPanic:" fmt "\033[0m \n", ##__VA_ARGS__); \
    exit(EXIT_FAILURE);                                                  \
  } while (0)

#define PANIC_ON(condition, message, ...) \
  do {                                    \
    if (condition) {                      \
      PANIC(message, ##__VA_ARGS__);      \
    }                                     \
  } while (0)

// Memory area for [@start, @end)
typedef struct {
  void *start, *end;
} Area;

int cpu_count();
int cpu_current();

#endif