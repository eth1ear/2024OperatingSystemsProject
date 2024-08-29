#include <common.h>

static void os_init() { pmm->init(); }

#ifndef TEST
static void os_run() {
  for (const char *s = "Hello World from CPU #*\n"; *s; s++) {
    putch(*s == '*' ? '0' + cpu_current() : *s);
  }
  while (1)
    ;
}

#else
static void os_run() {}
#endif

MODULE_DEF(os) = {
    .init = os_init,
    .run = os_run,
};
