#ifndef __COMMON_H
#define __COMMON_H

#include <kernel.h>
#include <klib-macros.h>
#include <klib.h>

typedef int lock_t;
#define PG_SZ (4096)
#define STACK_SIZE (4 * PG_SZ)

enum STATE { NEW, BLOCK, RUNNABLE, RUNNING, WAITING };

// Per-CPU state.
typedef struct cpu {
    int noff;    // Depth of push_off() nesting.
    int intena;  // Were interrupts enabled before push_off()?
} cpu_t;

struct spinlock {
    lock_t val;
    const char *name;
    cpu_t *cpu;
};

struct task {
    const char *name;
    int id;
    int suspend;
    int status;  // RUNABLE, RUNNING, BLOCK
    int block;
    int running;  // 允许调度
    struct task *next;
    Context *context;
    int fence1;
    uint8_t stack[STACK_SIZE];
    int fence2;
};

struct semaphore {
    spinlock_t lock;
    int count;
    const char *name;
    task_t *wait_list;  // not sure
};

typedef struct item {
    int seq;
    int event;
    handler_t handler;
} item_t;

// ------------------ macros -------------------
#define MAX_CPU 8
#define INT_MAX (2147483647)
#define INT_MIN (-INT_MAX - 1)
#define LOCK_INIT() (0)
#define FENCE (0xffffffff)
// ------------------ debug --------------------
#ifdef LOCAL_MACHINE
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...)
#endif

#endif
