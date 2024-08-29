#ifndef CO_H
#define CO_H
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "co.h"

// #define VERION_1
#ifdef VERION_1
#define Max_Co_Nr 4096
enum { CO_NEW, CO_RUNNING, CO_WAITING, CO_DEAD };

typedef struct co {
  /* data */
  int id;       // 协程的id
  int status;   // CO_NEW, CO_RUNNING, CO_WAITING, CO_DEAD
  jmp_buf ctx;  // 保存上下文
  struct co *waiter;

#define STK_SZ 64 * 1024
#define MAGIC 0xdeadbeef
  int fence1;  // 用于检测栈溢出
  char stack[STK_SZ] __attribute__((aligned(16)));
  int fence2;  // 用于检测栈溢出

  /* function */
  void (*func)(void *) __attribute__((aligned(16)));
  void *arg;
  char name[16];
} co_t;

#endif

#define VERION_2
#ifdef VERION_2

enum co_status {
  CO_NEW = 1,  // 新创建，还未执行过
  CO_RUNNING,  // 已经执行过
  CO_WAITING,  // 在 co_wait 上等待
  CO_DEAD,     // 已经结束，但还未释放资源
};

#define K 1024
#define STACK_SIZE (64 * K)

struct co {
  const char *name;
  void (*func)(void *);  // co_start 指定的入口地址和参数
  void *arg;

  enum co_status status;  // 协程的状态
  struct co *waiter;      // 是否有其他协程在等待当前协程
  jmp_buf context;        // 寄存器现场 (setjmp.h)
  unsigned char stack[STACK_SIZE];  // 协程的堆栈
};

typedef struct CONODE {
  struct co *coroutine;

  struct CONODE *fd, *bk;
} CoNode;
#endif

struct co *co_start(const char *name, void (*func)(void *), void *arg);
void co_yield ();
void co_wait(struct co *co);
typedef struct co co_t;

// For debug
#define LOCAL_MACHINE
#ifdef LOCAL_MACHINE
#define debug(...) printf(__VA_ARGS__)
#define Panic_On(cond, ...) \
  do {                      \
    if (cond) {             \
      printf(__VA_ARGS__);  \
      exit(1);              \
    }                       \
  } while (0)
#define Panic(...) Panic_On(1, __VA_ARGS__)
#else
#define debug(...)
#define Panic_On(cond, ...)
#define Panic(...)
#endif

#endif