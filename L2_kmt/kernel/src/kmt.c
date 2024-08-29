#include <common.h>
#include <os.h>

#define TRUE (1)
#define Max_P_Nr 256
task_t *tasks[Max_P_Nr] = {};
static task_t idle[MAX_CPU] = {};
static task_t *currents[MAX_CPU];  // 每个CPU的当前任务
// static task_t *buffers[MAX_CPU];   // 缓存任务，实际上就是上一个current
#define current currents[cpu_current()]
#define buffer buffers[cpu_current()]
static int Total_Nr = 0;     // 任务总数
static cpu_t cpus[MAX_CPU];  // 表示CPU的状态，主要用来记录锁的嵌套层数和最外层的锁前中断与否
static spinlock_t task_lk;

static void spin_init(spinlock_t *lk, const char *name);
static void spin_lock(spinlock_t *lk);
static void spin_unlock(spinlock_t *lk);
static int holding(spinlock_t *lk);                        // 判断锁是否重入
static void switch_boot_pcb();                             // 初始化idle进程的信息，并将current切换到idle
static Context *kmt_context_save(Event ev, Context *ctx);  // 上下文保存
static Context *kmt_schedule(Event ev, Context *ctx);      // 任务调度
static void kmt_init();
static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg);
static void kmt_teardown(task_t *task);
static void sem_init(sem_t *sem, const char *name, int value);
static void sem_wait(sem_t *sem);
static void sem_signal(sem_t *sem);

MODULE_DEF(kmt) = {.init = kmt_init,
                   .create = kmt_create,
                   .teardown = kmt_teardown,
                   .spin_init = spin_init,
                   .spin_lock = spin_lock,
                   .spin_unlock = spin_unlock,
                   .sem_init = sem_init,
                   .sem_wait = sem_wait,
                   .sem_signal = sem_signal};

/******************** tasks management ************************/
static void switch_boot_pcb() {
    for (int i = 0; i < cpu_count(); i++) {
        currents[i] = &idle[i];
        currents[i]->name = "Idle";
        currents[i]->status = RUNNING;
        currents[i]->block = 0;
        currents[i]->suspend = 0;
        currents[i]->fence1 = FENCE;
        currents[i]->fence2 = FENCE;
        currents[i]->id = 0;
    }
}

// Context保存在stack的最底部（kcontext已经做了初始化）
static Context *kmt_context_save(Event ev, Context *ctx) {
    panic_on(ienabled() != false, "应该关中断!");
    panic_on((current->fence1 != FENCE || current->fence2 != FENCE), "stack overflow!");
    if (current->status != BLOCK) current->status = RUNNABLE;
    current->context = ctx;
    return NULL;
}

static Context *kmt_schedule(Event ev, Context *ctx) {
    int index = current->id, i = 0;
    while (i < Total_Nr) {
        index = (index + 1) % Total_Nr;
        if (tasks[index]->status == RUNNABLE) break;
        i++;
    }
    if (i == Total_Nr) {  // 使用idle进程
        current = &idle[cpu_current()];
    } else {
        current = tasks[index];
    }
    current->status = RUNNING;
    panic_on((current->fence1 != FENCE || current->fence2 != FENCE), "stack overflow!");
    return current->context;
}

void kmt_init() {
    os->on_irq(INT_MIN, EVENT_NULL, kmt_context_save);
    os->on_irq(INT_MAX, EVENT_NULL, kmt_schedule);
    switch_boot_pcb();
    spin_init(&task_lk, "task lock");
}

int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg) {
    spin_lock(&task_lk);
    task->name = name;
    task->status = RUNNABLE;
    task->suspend = 0;
    task->block = 0;
    task->running = 0;
    task->fence1 = FENCE;
    task->fence2 = FENCE;  // 防止栈溢出
    Area stack = (Area){task->stack, task->stack + STACK_SIZE};
    task->context = kcontext(stack, entry, arg);
    task->id = Total_Nr;
    tasks[Total_Nr] = task;
    Total_Nr++;
    spin_unlock(&task_lk);
    return 0;
}

void kmt_teardown(task_t *task) { panic("Not implemented!"); }

/******************** spin lock ************************/

// 任何其他线程、中断处理程序、其他处理器都不能同时得到同一把锁
// Taken from xv6
// Check whether this cpu is holding the lock.
// Interrupts must be off.

void lockPanic(spinlock_t *lk, const char *fmt, ...) {  // simply for debugging
    char out[2048];
    va_list va;
    va_start(va, fmt);
    vsprintf(out, fmt, va);
    va_end(va);
    putstr(out);
    panic("lock 的错误：异常退出!");
}

static int holding(spinlock_t *lk) {
    int r = (lk->val && lk->cpu == &cpus[cpu_current()]);
    return r;
}

static void push_off(spinlock_t *lk) {
    int old = ienabled();  // 获取此时的中断开启与否
    iset(false);           // 硬件上关闭中断
    int id = cpu_current();
    if (cpus[id].noff == 0) cpus[id].intena = old;  // 记录最外层的CPU的中断标记
    cpus[id].noff += 1;                             // 中断的嵌套层数加一
}

static void pop_off(spinlock_t *lk) {
    if (ienabled()) lockPanic(lk, "pop_off - interruptible");
    int id = cpu_current();
    if (cpus[id].noff < 1) lockPanic(lk, "pop_off");
    cpus[id].noff -= 1;
    if (cpus[id].noff == 0 && cpus[id].intena) iset(true);
}

static void spin_init(spinlock_t *lk, const char *name) {
    lk->val = 0;
    lk->name = name;  // Not Sure
    lk->cpu = NULL;
}

static void spin_lock(spinlock_t *lk) {
    push_off(lk);
    if (holding(lk)) lockPanic(lk, "acquire! lock's name is %s, cur-cpu = %d\n", lk->name, cpu_current());
    while (atomic_xchg(&lk->val, 1))
        ;
    __sync_synchronize();
    lk->cpu = &cpus[cpu_current()];
}

static void spin_unlock(spinlock_t *lk) {
    if (!holding(lk))
        lockPanic(lk, "Should acquire the lock! current CPU = #%d lk->cpu = %d\n", cpu_current(), lk->cpu);
    lk->cpu = NULL;
    __sync_synchronize();
    atomic_xchg(&lk->val, 0);
    pop_off(lk);
}

/******************** semaphore *************************/
static void sem_init(sem_t *sem, const char *name, int value) {
    sem->name = name;
    sem->count = value;  // init
    spin_init(&(sem->lock), NULL);
    assert(sem->wait_list == NULL);
}

static void sem_wait(sem_t *sem) {
    int Flag = 0;
    spin_lock(&(sem->lock));
    sem->count--;
    if (sem->count < 0) {                   // 说明当前的线程不能够继续执行了
        atomic_xchg(&(current->block), 1);  // 将当前的任务的状态修改为不可执行
        current->status = BLOCK;
        current->next = sem->wait_list;
        sem->wait_list = current;
        Flag = 1;
    }
    spin_unlock(&(sem->lock));
    if (Flag) {
        panic_on(ienabled() == false, "不应该关中断!");
        yield();  // 切换到其他进程
    }
}

// 取出当前队列的一个元素,同时标记为可执行
static void sem_signal(sem_t *sem) {
    spin_lock(&(sem->lock));
    sem->count++;
    if (sem->count <= 0) {  // 说明原先的count严格小于 0
        assert(sem->wait_list);
        task_t *head = sem->wait_list;
        sem->wait_list = head->next;
        atomic_xchg(&(head->block), 0);  // 将当前的任务的状态修改为可执行
        head->status = RUNNABLE;
    }
    spin_unlock(&(sem->lock));
}
