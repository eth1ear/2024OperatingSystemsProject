#ifndef _THREAD_H
#define _THREAD_H

// Spinlock
typedef int lock_t;
#define LOCK_INIT() 0

void lock(lock_t *lk) {
  while (1) {
    intptr_t value = atomic_xchg(lk, 1);
    if (value == 0) {
      break;
    }
  }
  debug("Obtain Lock successfully\n");
}
void unlock(lock_t *lk) {
  atomic_xchg(lk, 0);
  debug("Restore Lock successfully\n");
}

#endif
