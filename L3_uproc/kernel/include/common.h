#ifndef __COMMON_H
#define __COMMON_H

#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>

// ------------------ macros -------------------
#define MAX_CPU 8
#define INT_MAX (2147483647)
#define INT_MIN (-INT_MAX-1)
#define LOCK_INIT() (0)
#define FENCE (0xffffffff)
#define TAG (0x55555555)
#define MAX_INTR (8)
typedef int lock_t;
#define STACK_SIZE (8192)
#define PG_NR (32)
#define KB (1024)
#define MB (1024 * KB)
#define GB (1024 * MB)

#define TASK_INIT(task) do { \
  task->valid = TAG;\
  task->suspend = 0; \
  task->block = 0;\
  task->fence1 = task->fence2 = FENCE;\
  task->running = 0;\
  task->ntrap = 0; \
  task->next = NULL;\
  task->dead = 0;\
  task->parent = NULL; task->chldlist = NULL;\
  task->presib = task->nxtsib = NULL;\
  task->xclist = NULL;\
  kmt->spin_init(&task->pro_lk, NULL);\
} while(0)

// Per-CPU state.
typedef struct cpu {
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
}cpu_t;

struct spinlock {
  lock_t val;
  const char * name;
  cpu_t* cpu;
};

// child list : to record info of exited child process
typedef struct xchld{
  int pid; // 子进程的pid
  int xstatus; // 子进程退出的状态记录
  struct xchld* nxt;
  struct xchld* pre;
}xchld_t; // exit child status: xchld

// address space element
typedef struct adrspc {
  spinlock_t  adrlk; // 防止竞争
  Area area;
  int refcnt; // 表示该地址空间的引用计数
  int pgnr; // 页面数目
  void * va[PG_NR]; // 属于该地址空间的虚拟地址
  void * pa[PG_NR]; // 属于该地址空间的物理地址
  int prot;   // 记录某一个进程对于某一个地址空间的权限，对于共享地址空间，该值一旦确定好之后就不再变化
  int share; // 该地址空间是否共享
}adrspc_t;

struct task {
  int             valid; // for debugging, initialized as TAG.(0x55555555)
  const char      *name; // for debugging
  int             id; // pid, equal to its index of tasks
  int             suspend; // 防止栈抢占问题
  int             block; // used in semaphore
  struct task     *next; // used in semaphore
  int             dead; // either exit() or kill() will change the value to 1
  int             running; // 0: ok to be scheduled if stack is safe 1: should not be scheduled
  Context         *context[MAX_INTR]; // 考虑到嵌套的问题,上下文需要保存为一个数组
  int             ntrap; // trap嵌套层数 number of trap， Depth of os_trap() nesting
  AddrSpace       as;  // 地址空间，用户线程独有
  // stack 对于用户程序而言实际上就是内核栈（存储上下文结构），
  // 对于内核线程而言，内核线程的地址空间与kernel保持一致，stack就是运行栈
  int             fence1;
  uint8_t         stack[STACK_SIZE];
  int             fence2;
  // 父子进程 :
  spinlock_t      pro_lk; // process management lock
  struct task     *parent; // 父进程
  struct task     *chldlist; // 子进程的链表
  struct task     *presib;  // previous sibling：前一个兄弟进程
  struct task     *nxtsib;  // next sibling： 后一个兄弟进程
  xchld_t         *xclist; //  exit child list：链表记录子进程的退出状态, used for wait()
  // 资源回收：
  int             xstatus; // 退出状态
  int             gabage; // 1：可以回收， 0： 不能回收
  int             cpu; // 回收必须绑定CPU
  // mmap : addr space management
  void *          fraddr; // [fraddr, MAX)是mmap未分配区域
  int             adrnr;  // 地址段的个数：初始化为2，代码段和栈区
  adrspc_t        *adrlist[128]; // 当前的进程所拥有的地址空间的数组（空间碎片链表）   
};


struct semaphore {
  spinlock_t lock;
  int count;
  const char * name;
  task_t * wait_list; // not sure
};

#define TOTPGNR (32000) // max page number
typedef struct cpy_wrt { // copy-on-write
  int pgcnt[TOTPGNR]; // totoal page nr
}cpy_wrt_t;

typedef struct item {
  int seq;
  int event;
  handler_t handler;
}item_t;

// ------------------- joint APIs --------------------
task_t * current_proc(); // 主要是给uproc提供接口，current process
void  addTask(task_t * task); // 添加任务
task_t * getTask(int pid); // get task by id
void inc_pgcnt(void* pa); // increment page count
void dec_pgcnt(void* pa); // decrease page cnt
// ------------------ debug --------------------
#ifdef LOCAL_MACHINE
  #define debug(...) printf(__VA_ARGS__)
  #define Log(format, ...) \
  printf("\33[1;36m[%s,%d,%s] " format "\33[0m\n", \
      __FILE__, __LINE__, __func__, ## __VA_ARGS__)
#else
  #define debug(...)
  #define Log(...)
#endif

#endif
