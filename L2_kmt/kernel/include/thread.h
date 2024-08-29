// Spinlock for alloc and free, Not for kmt
typedef int lock_t;

void lock(lock_t *lk) {
  while (1) {
    intptr_t value = atomic_xchg(lk, 1);
    if (value == 0) {
      break;
    }
  }
}
void unlock(lock_t *lk) {
  atomic_xchg(lk, 0);
}


