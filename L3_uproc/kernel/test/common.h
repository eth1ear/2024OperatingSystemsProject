#include <kernel.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#ifdef LOCAL_MACHINE
  #define debug(...) printf(__VA_ARGS__)
#else
  #define debug(...)
#endif

// Memory area for [@start, @end)
typedef struct {
  void *start, *end;
} Area;

int cpu_count();
int cpu_current();