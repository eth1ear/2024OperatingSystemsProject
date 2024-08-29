#include "co.h"

#ifdef VERION_1
int Total_Nr = 0;
static struct co *co_arrary[Max_Co_Nr] __attribute__((used)) = {};
static struct co *current = NULL;

/**
 * @brief 协程的执行函数(used in co_yield)，执行完毕后将状态更改为 CO_DEAD
 */
void *func_wraper(void *arg) {
  co_t *co = (co_t *)arg;
  co->func(co->arg);
  co->status = CO_DEAD;  // 执行完毕
  if (co->waiter)
    co->waiter->status = CO_RUNNING;  // 将原来的WAITING 状态更改为运行状态
  co_yield ();
  return NULL;
}

/*
 * 切换栈，即让选中协程的所有堆栈信息在自己的堆栈中，而非调用者的堆栈。保存调用者需要保存的寄存器，并调用指定的函数
 */
static inline void stack_switch_call(void *sp, void *entry, void *arg) {
  asm volatile(
#if __x86_64__
      "movq %%rcx, 0(%0); movq %0, %%rsp; movq %2, %%rdi; call *%1"
      :
      : "b"((uintptr_t)sp - 16), "d"((uintptr_t)entry), "a"((uintptr_t)arg)
#else
      "movl %%ecx, 4(%0); movl %0, %%esp; movl %2, 0(%0); call *%1"
      :
      : "b"((uintptr_t)sp - 8), "d"((uintptr_t)entry), "a"((uintptr_t)arg)
#endif
  );
}

/**
 * @brief 创建一个协程(使用数组只是简单起见，可以选择将CO_DEAD的进行复用)
 */
struct co *co_start(const char *name, void (*func)(void *), void *arg) {
  int i = 0;
  for (i = 0; i < Total_Nr; i++) {
    // 如果数组当中的某个元素已经free(已经dead),那么可以“复用”
    if (co_arrary[i]->status == CO_DEAD) {
      break;
    }
  }

  co_t *co = NULL;
  if (i == Total_Nr) {
    // 没办法复用的时候，需要申请新的资源
    co = (co_t *)malloc(sizeof(struct co));
    co->id = Total_Nr;
    co_arrary[Total_Nr++] = co;
  } else {
    // 已经dead的资源复用
    co = co_arrary[i];
  }

  co->status = CO_NEW;
  memset(co->stack, 0, STK_SZ);
  co->fence1 = co->fence2 = MAGIC;
  co->waiter = NULL;
  co->func = func;
  co->arg = arg;
  strcpy(co->name, name);

  return co;
}

/**
 * @brief 等待协程结束
 */
void co_wait(struct co *co) {
  if (co->status == CO_DEAD) {
    return;
  }
  Panic_On(current == NULL, "current is NULL");
  current->status = CO_WAITING;
  co->waiter = current;  // 当前的协程需要等待 co 的结束
  while (co->status != CO_DEAD) {
    co_yield ();
  }
}

/**
 * @brief 切换到下一个协程
 */
void co_yield () {
  Panic_On(current == NULL, "current is NULL");
  Panic_On(current->fence1 != MAGIC || current->fence2 != MAGIC,
           "stack overflow");
  int val = setjmp(current->ctx);  // 保存当前的上下文
  if (val == 0) {
    // USING ROUND ROBIN TO SCHEDULE
    int next = (current->id + 1) % Total_Nr;
    for (int i = 0; i < Total_Nr; i++) {
      if (co_arrary[next]->status == CO_NEW ||
          co_arrary[next]->status == CO_RUNNING) {
        break;
      }
      next = (next + 1) % Total_Nr;
    }
    current = co_arrary[next];
    if (current->status == CO_NEW) {
      current->status = CO_RUNNING;
      stack_switch_call(current->stack + STK_SZ, (void *)func_wraper, current);
    } else {
      longjmp(current->ctx, 1);
    }
  }
  Panic_On(!current && (current->fence1 != MAGIC || current->fence2 != MAGIC),
           "stack overflow");
}

/**
 * @brief 初始化协程，创建一个名为 init 的协程
 */
void __attribute__((constructor)) co_init() {
  co_t *init = co_start("init", NULL, NULL);
  init->status = CO_RUNNING;
  current = init;
}

/**
 * @brief 退出时释放所有协程的资源
 */
void __attribute__((destructor)) co_exit() {
  for (int i = 0; i < Total_Nr; i++) {
    free(co_arrary[i]);
  }
}

#endif  // VERION_1

#ifdef VERION_2

struct co *current;

static CoNode *co_node = NULL;
/*
 * 如果co_node == NULL，则创建一个新的双向循环链表即可，并返回
 * 如果co_node != NULL, 则在co_node和co_node->fd之间插入，仍然返回co_node的值
 */
static void co_node_insert(struct co *coroutine) {
  CoNode *victim = (CoNode *)malloc(sizeof(CoNode));
  assert(victim);

  victim->coroutine = coroutine;
  if (co_node == NULL) {
    victim->fd = victim->bk = victim;
    co_node = victim;
  } else {
    victim->fd = co_node->fd;
    victim->bk = co_node;
    victim->fd->bk = victim->bk->fd = victim;
  }
}

/*
 * 如果当前只剩node一个，则返回该一个
 * 否则，拉取当前co_node对应的协程，并沿着bk方向移动
 */
static CoNode *co_node_remove() {
  CoNode *victim = NULL;

  if (co_node == NULL) {
    return NULL;
  } else if (co_node->bk == co_node) {
    victim = co_node;
    co_node = NULL;
  } else {
    victim = co_node;

    co_node = co_node->bk;
    co_node->fd = victim->fd;
    co_node->fd->bk = co_node;
  }

  return victim;
}

struct co *co_start(const char *name, void (*func)(void *), void *arg) {
  struct co *coroutine = (struct co *)malloc(sizeof(struct co));
  assert(coroutine);

  coroutine->name = name;
  coroutine->func = func;
  coroutine->arg = arg;
  coroutine->status = CO_NEW;
  coroutine->waiter = NULL;

  co_node_insert(coroutine);
  return coroutine;
}

void co_wait(struct co *coroutine) {
  assert(coroutine);

  if (coroutine->status != CO_DEAD) {
    coroutine->waiter = current;
    current->status = CO_WAITING;
    co_yield ();
  }

  /*
   * 释放coroutine对应的CoNode
   */
  while (co_node->coroutine != coroutine) {
    co_node = co_node->bk;
  }

  assert(co_node->coroutine == coroutine);

  free(coroutine);
  free(co_node_remove());
}

/*
 * 切换栈，即让选中协程的所有堆栈信息在自己的堆栈中，而非调用者的堆栈。保存调用者需要保存的寄存器，并调用指定的函数
 */
static inline void stack_switch_call(void *sp, void *entry, void *arg) {
  asm volatile(
#if __x86_64__
      "movq %%rcx, 0(%0); movq %0, %%rsp; movq %2, %%rdi; call *%1"
      :
      : "b"((uintptr_t)sp - 16), "d"((uintptr_t)entry), "a"((uintptr_t)arg)
#else
      "movl %%ecx, 4(%0); movl %0, %%esp; movl %2, 0(%0); call *%1"
      :
      : "b"((uintptr_t)sp - 8), "d"((uintptr_t)entry), "a"((uintptr_t)arg)
#endif
  );
}
/*
 * 从调用的指定函数返回，并恢复相关的寄存器。此时协程执行结束，以后再也不会执行该协程的上下文。这里需要注意的是，其和上面并不是对称的，因为调用协程给了新创建的选中协程的堆栈，则选中协程以后就在自己的堆栈上执行，永远不会返回到调用协程的堆栈。
 */
static inline void restore_return() {
  asm volatile(
#if __x86_64__
      "movq 0(%%rsp), %%rcx"
      :
      :
#else
      "movl 4(%%esp), %%ecx"
      :
      :
#endif
  );
}

#define __LONG_JUMP_STATUS (1)
void co_yield () {
  int status = setjmp(current->context);
  if (!status) {
    // 此时开始查找待选中的进程，因为co_node应该指向的就是current对应的节点，因此首先向下移动一个，使当前线程优先级最低
    co_node = co_node->bk;
    while (!((current = co_node->coroutine)->status == CO_NEW ||
             current->status == CO_RUNNING)) {
      co_node = co_node->bk;
    }

    assert(current);

    if (current->status == CO_RUNNING) {
      longjmp(current->context, __LONG_JUMP_STATUS);
    } else {
      ((struct co volatile *)current)->status =
          CO_RUNNING;  // 这里如果直接赋值，编译器会和后面的覆写进行优化

      // 栈由高地址向低地址生长
      stack_switch_call(current->stack + STACK_SIZE, current->func,
                        current->arg);
      // 恢复相关寄存器
      restore_return();

      // 此时协程已经完成执行
      current->status = CO_DEAD;

      if (current->waiter) {
        current->waiter->status = CO_RUNNING;
      }
      co_yield ();
    }
  }

  assert(
      status &&
      current->status ==
          CO_RUNNING);  // 此时一定是选中的进程通过longjmp跳转到的情况执行到这里
}

static __attribute__((constructor)) void co_constructor(void) {
  current = co_start("main", NULL, NULL);
  current->status = CO_RUNNING;
}

static __attribute__((destructor)) void co_destructor(void) {
  if (co_node == NULL) {
    return;
  }

  while (co_node) {
    current = co_node->coroutine;
    free(current);
    free(co_node_remove());
  }
}
#endif  // VERION_2