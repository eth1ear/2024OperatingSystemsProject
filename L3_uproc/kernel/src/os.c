#include <common.h>
#include <os.h>
#include <devices.h>

static inline task_t *task_alloc() {
  return pmm->alloc(sizeof(task_t));
}
// 测试一
// #define TEST_1
void print(void * arg) {
  char *c = (char*) arg;
  while(1) {
    putch(*c);
    for(volatile int i = 0; i < 100000; i++);
  }
}
// 测试二
// #define TEST_2
static spinlock_t lk1;
static spinlock_t lk2;
void lock_test(void* arg) {
  int *intr = (int*) arg;
  // intr = 0, 关中断, ienabled() = false
  // intr = 1， 开中断, ienabled() = false
  // ABAB 形的锁测试
  if(!*intr)iset(false);
  else iset(true);
  kmt->spin_lock(&lk1);
  for(volatile int i = 0; i < 100000; i++);
  panic_on(ienabled() == true, "不应该开中断！");
  kmt->spin_lock(&lk2);
  for(volatile int i = 0; i < 100000; i++);
  panic_on(ienabled() == true, "不应该开中断！");
  kmt->spin_unlock(&lk1);
  for(volatile int i = 0; i < 100000; i++);
  panic_on(ienabled() == true, "不应该开中断！");
  kmt->spin_unlock(&lk2);
  for(volatile int i = 0; i < 100000; i++);
  panic_on(ienabled() != *intr, "中断恢复错误！");
  printf("pass test for ABAB lock, 锁的初始状态是：[%d]\n", *intr);
  if(!*intr)iset(false);
  else iset(true);
  kmt->spin_lock(&lk1);
  for(volatile int i = 0; i < 100000; i++);
  panic_on(ienabled() == true, "不应该开中断！");
  kmt->spin_lock(&lk2);
  for(volatile int i = 0; i < 100000; i++);
  panic_on(ienabled() == true, "不应该开中断！");
  kmt->spin_unlock(&lk1);
  for(volatile int i = 0; i < 100000; i++);
  panic_on(ienabled() == true, "不应该开中断！");
  kmt->spin_unlock(&lk2);
  for(volatile int i = 0; i < 100000; i++);
  panic_on(ienabled() != *intr, "中断恢复错误！");
  printf("pass test for ABBA lock, 锁的初始状态是：[%d]\n", *intr);
  iset(true);
  while(1) ;
}

// 测试三
// #define TEST_3 
#define P kmt->sem_wait
#define V kmt->sem_signal
sem_t empty, fill;
void producer(void *arg) { while (1) { P(&empty); putch('('); V(&fill);  } }
void consumer(void *arg) { while (1) { P(&fill);  putch(')'); V(&empty); } }

// 测试四：设备测试
// #define TEST_4
#ifdef TEST_4
static void tty_reader(void *arg) {
  printf("%s\n", (char *)arg);
  device_t *tty = dev->lookup(arg);
  char cmd[128], resp[128], ps[16];
  snprintf(ps, 16, "(%s) $ ", arg);
  while (1) {
    tty->ops->write(tty, 0, ps, strlen(ps));
    int nread = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
    cmd[nread] = '\0';
    sprintf(resp, "tty reader task: got %d character(s).\n", strlen(cmd));
    tty->ops->write(tty, 0, resp, strlen(resp));
  }
}
#endif

// 用户程序测试

static void os_init() {
  pmm->init();
  kmt->init();
  uproc->init(); 
// 测试一: 简单测试，中断"c","d"交替出现
#ifdef TEST_1
  kmt->create(task_alloc(), "a", print, "c");
  kmt->create(task_alloc(), "b", print, "d");
#endif

// 测试二： 锁的测试
#ifdef TEST_2
  static int zero = 0, one = 1;
  kmt->spin_init(&lk1, NULL);
  kmt->spin_init(&lk2, NULL);
  int * arg0 = &zero;
  int * arg1 = &one;
  kmt->create(task_alloc(), "lock_test", lock_test, (void*)arg0);
  kmt->create(task_alloc(), "lock_test", lock_test, (void*)arg1);
#endif

#ifdef TEST_3
  kmt->sem_init(&empty, "empty", 5);  // 缓冲区大小为 5
  kmt->sem_init(&fill,  "fill",  0);
  for (int i = 0; i < 4; i++) // 4 个生产者
    kmt->create(task_alloc(), "producer", producer, NULL);
  for (int i = 0; i < 5; i++) // 5 个消费者
    kmt->create(task_alloc(), "consumer", consumer, NULL);
#endif

#ifdef TEST_4
  dev->init();
  kmt->create(task_alloc(), "tty_reader", tty_reader, "tty1");
  kmt->create(task_alloc(), "tty_reader", tty_reader, "tty2");
#endif

}

#ifndef TEST
static void os_run() {
  iset(true);
  yield();
  while(1);
}

#else
static void os_run() {
}
#endif

#define Max_Nr 128
static item_t tab[Max_Nr];
static int Item_Nr = 0;
static Context* os_trap(Event ev, Context *ctx) {
  panic_on(ienabled() != false, "应该关中断!");
  Context * next = NULL;
  for(int i = 0; i < Item_Nr; i++) {
    if(tab[i].event == EVENT_NULL || tab[i].event == ev.event) {
      Context* ret = tab[i].handler(ev, ctx);
      panic_on(ret && next, "returning multiple contexts");
      if(ret) next = ret;
    }
  }
  panic_on(!next, "returning NULL context");
  return next;
}

#define swap(x,y) do {   \
  typeof(x) _x = x;      \
  typeof(y) _y = y;      \
  x = _y;                \
  y = _x;                \
} while(0) 

static void os_on_irq(int seq, int event, handler_t handler) {
  tab[Item_Nr] = (item_t) { .seq = seq, .event = event, .handler = handler };
  Item_Nr++;
  for(int i = 0; i < Item_Nr; i++) { // 冒泡排序
    for(int j = i + 1; j < Item_Nr; j++) {
      if(tab[i].seq > tab[j].seq) {
        swap(tab[i].seq, tab[j].seq);
        swap(tab[i].event, tab[j].event);
        swap(tab[i].handler, tab[j].handler);
      }
    } 
  }
}

MODULE_DEF(os) = {
  .init = os_init,
  .run  = os_run,
  .trap = os_trap, // 因为每个处理器都会被中断，这让 os->trap() 变成了被并行执行的代码。
  .on_irq = os_on_irq,
};