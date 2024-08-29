#include <os.h>
#include <syscall.h>
#include <user.h>
#include <common.h>
#include "initcode.inc"

static spinlock_t wait_lk; // used for exit and wait

static void init();
static int kputc(task_t *task, char ch);
static int fork(task_t *task);
static int wait(task_t *task, int *status);
static int exit(task_t *task, int status);
static int kill(task_t *task, int pid);
static void* mmap(task_t *task, void *addr, int length, int prot, int flags);
static int getpid(task_t *task);
static int sleep(task_t *task, int seconds);
static int64_t uptime(task_t *task);
static Context* syscall(Event ev, Context* ctx) ; // 系统调用
static Context* pagefault(Event ev, Context* ctx) ; // 缺页异常处理函数
static task_t * uproc_create(char * name, int runnable); // 创建用户进程
static adrspc_t * addralloc(void * start, void * end, int prot, int share);// 申请并初始化地址段元素

cpy_wrt_t refcnt; 
static spinlock_t pg_lk; // to manage the refcnt  
void inc_pgcnt(void* pa) { // increment page count
    int index = (uintptr_t)(pa - heap.start) / (4096); // the index of pa in refcnt
    panic_on(index >= TOTPGNR || index < 0, "invalid physical addr");
    kmt->spin_lock(&pg_lk);
    refcnt.pgcnt[index] += 1;
    kmt->spin_unlock(&pg_lk);
}
void dec_pgcnt(void* pa) { // decrease page cnt
    int index = (uintptr_t)(pa - heap.start) / (4096);
    panic_on(index >= TOTPGNR || index < 0, "invalid physical addr");
    panic_on(refcnt.pgcnt[index] < 1, "decrease cnt fail");
    kmt->spin_lock(&pg_lk);
    refcnt.pgcnt[index] -= 1;
    if(refcnt.pgcnt[index] == 0) pmm->free(pa); // free the page iff. page cnt is zero
    kmt->spin_unlock(&pg_lk);
}

MODULE_DEF(uproc) = {
	.init = init,
	.kputc = kputc,
	.fork = fork,
	.wait = wait,
	.exit = exit,
	.kill = kill,
	.mmap = mmap,
	.getpid = getpid,
	.sleep = sleep,
	.uptime = uptime
};

static void init() {
	vme_init(pmm->alloc, pmm->free);
    os->on_irq(1, EVENT_SYSCALL,    syscall);
    os->on_irq(0, EVENT_PAGEFAULT,  pagefault); // 不太确定
    kmt->spin_init(&wait_lk, "wait lock");
    kmt->spin_init(&pg_lk, "page lock");
    Log("[code length]:%d Bytes", _init_len);
	uproc_create("init", 1);  // 创建初始化用户进程
}

static int kputc(task_t *task, char ch) {
  	putch(ch); // safe for qemu even if not lock-protected
  	return 0;
}

static int fork(task_t *task) {
    /* 实现了copy-on-write */
    task_t * parent = task; // 父进程
    task_t * child = uproc_create(NULL, 0); // 创建子进程，尚且不允许调度
    child->parent = parent;

    // 将新建的任务添加到parent的子进程的链表中
    kmt->spin_lock(&task->pro_lk);
    child->nxtsib = parent->chldlist;
    if(parent->chldlist)parent->chldlist->presib = child;
    parent->chldlist = child;
    kmt->spin_unlock(&task->pro_lk);

    uintptr_t rsp0 = child->context[0]->rsp0; // 内核栈独有
    void * cr3 = child->context[0]->cr3; // 页目录的基地址独有
    memcpy(child->context[0], parent->context[0], sizeof(Context)); // Be careful
    child->context[0]->cr3 = cr3;
    child->context[0]->GPRx = 0; // 子进程返回值
    child->context[0]->rsp0 = rsp0;

    child->fraddr = parent->fraddr; // free address 
    child->adrnr = parent->adrnr;
    adrspc_t ** adrlist = task->adrlist;
    adrspc_t * space = NULL;
    for(int i = 0; i < parent->adrnr; i++) { // 遍历父进程的地址空间
        space = adrlist[i];
        kmt->spin_lock(&space->adrlk);
        if(space->share == 1) space->refcnt++; // 共享地址段引用计数加一
        adrspc_t * newspc = (space->share == 1 || child->adrlist[i] == NULL) ? space : pmm->alloc(sizeof(adrspc_t)); // 如果共享，那么使用同一个指针，否则申请之后memcpy
        kmt->spin_unlock(&space->adrlk);
        if(!space->share) { // 将非共享映射页面添加到子进程的地址空间
            newspc->pgnr = space->pgnr; newspc->share = space->share; newspc->prot = space->prot;
            newspc->area = (Area){space->area.start, space->area.end};
            for(int j = 0; j < space->pgnr; j++) {
                void * va = space->va[j];
                void * pa = space->pa[j];
                newspc->va[j] = va;
                newspc->pa[j] = pa;
                if(space->prot & PROT_WRITE) { map(&parent->as, va, pa, PROT_NONE); map(&parent->as, va, pa, PROT_READ); } // 将父进程中写权限页面转化为只读页面
                map(&child->as, va, pa, PROT_READ);
                inc_pgcnt(pa); // 将该页面的引用计数加一
            }
        }
        child->adrlist[i] = newspc;
    }
    child->running = 0; // 此时才可调度
    return child->id;
}

static int wait(task_t *task, int *status) {
    // The wait() system call suspends execution of the calling thread
    // until one of its children terminates.
    // wait(): on success, returns the process ID of the terminated
    // child; on failure, -1 is returned.
    int pid = -1;
    kmt->spin_lock(&wait_lk);
    while(1) {
        xchld_t* item = task->xclist;
        if(item) {
            pid = item->pid;
            if(status)*status = item->xstatus;
            if(item->pre) item->pre->nxt = item->nxt;
            if(item->nxt) item->nxt->pre = item->pre;
            if(item == task->xclist)task->xclist = item->nxt;
            kmt->spin_unlock(&wait_lk);
            return pid;
        }
        task_t* proc = task->chldlist;
        if(proc == NULL) {
            kmt->spin_unlock(&wait_lk);
            return -1; // 没有子进程了
        } else {
            kmt->spin_unlock(&wait_lk);
            yield();
            kmt->spin_lock(&wait_lk); // 在返回之后需要重新获得等待锁，其实yield + 这一行相当于sleeplock
        }
    }
    // 不应该在此时持有wait锁！！
    panic_on(pid == -1, "invalid return value!");
    return -1;
}

static int exit(task_t *task, int status) {
    task->xstatus = status << 8; // 记录线程退出状态
    kmt->spin_lock(&wait_lk);
    task_t* parent = task->parent;
    if(parent == NULL) {
    } else {
        // 将自己的退出的状态记录到父进程的数组中
        panic_on(parent->valid != TAG, "invalid parent!"); // 父进程结构体无效

        xchld_t* item = pmm->alloc(sizeof(xchld_t)); // exit child status: xchld
        item->pre = item->nxt = NULL; // init 
        item->pid = task->id; 
        item->xstatus = task->xstatus; 

        xchld_t* header = parent->xclist; // exit child list: insert item into the list, be careful 
        item->nxt = header;
        if(header)header->pre = item;
        parent->xclist = item;

        // 将自己移出父进程的childlist, 需要十分十分小心，非常容易出错
        kmt->spin_lock(&parent->pro_lk);
        if(task->presib) task->presib->nxtsib = task->nxtsib; 
        if(task->nxtsib) task->nxtsib->presib = task->presib;
        if(task == parent->chldlist) {
            parent->chldlist = task->nxtsib;
        }
        kmt->spin_unlock(&parent->pro_lk);
    }

    task_t* proc = task->chldlist; // 每一个任务结构体都有一个子进程链表，存储其所有的child
    while(proc) {  // 枚举所有的child
        proc->parent = NULL; // 将子进程的 parent 标记为NULL
        proc = proc->nxtsib;
    }
    task->dead = task->gabage = 1;
    task->cpu = cpu_current();
    kmt->spin_unlock(&wait_lk);
    assert(task->dead == 1 && task->gabage == 1);
    return status;
}

static int kill(task_t *task, int pid) {
	// 可能需要kmt提供一个接口获得task，并将task->killed改为1
    task_t * ktsk = getTask(pid); // get task by id , killed task: ktsk
    kmt->spin_lock(&wait_lk);
    task_t* parent = ktsk->parent; // 获得进程的父进程
    if(parent == NULL) { // does nothing
    } else {
        // 将自己的退出的状态记录到父进程的数组中
        panic_on(parent->valid != TAG, "invalid parent!"); // 父进程结构体无效
        // 将自己移出父进程的childlist, 需要十分十分小心，非常容易出错
        kmt->spin_lock(&parent->pro_lk);
        if(ktsk->presib) ktsk->presib->nxtsib = ktsk->nxtsib; 
        if(ktsk->nxtsib) ktsk->nxtsib->presib = ktsk->presib;
        if(ktsk == parent->chldlist) parent->chldlist = ktsk->nxtsib;
        kmt->spin_unlock(&parent->pro_lk);
    }
    task_t* proc = ktsk->chldlist; // 每一个任务结构体都有一个子进程链表，存储其所有的child
    while(proc) {  // 枚举所有的child
        proc->parent = NULL; // 将子进程的 parent 标记为NULL
        proc = proc->nxtsib;
    }
    ktsk->dead = ktsk->gabage = 1;
    kmt->spin_unlock(&wait_lk);
    assert(ktsk->dead == 1 && ktsk->gabage == 1);
	return 0; // 随意指定返回值嘛！
}

static void* _mmap(task_t *task, void *addr, int length, int prot, int flags) { // do not actually alloc physical pages, simply add a tags for this addrspace
    void * fraddr = task->fraddr; // [fraddr, max) 这一部分地址空间为允许使用空间
    int pgsize = task->as.pgsize;
    int share = (flags == MAP_SHARED) ? 1 : 0;
    length = (int)UPROUND(length, pgsize); // 上对齐到页面大小
    addr = (void*)UPROUND((uintptr_t)addr, pgsize); // 上对齐到页面大小
    void * ret = ((uintptr_t)fraddr >= (uintptr_t)addr) ? fraddr : addr; 
    task->fraddr = ret + length; // 仔细思考
    panic_on((uintptr_t)task->fraddr > (uintptr_t)task->as.area.end, "mmap");
    adrspc_t * space = addralloc(ret, ret + length, prot, share); 
    task->adrlist[task->adrnr] = space;
    task->adrnr++;
    // Log("[%p,%p)  #%d", ret, ret + length, task->id);
    return ret;
}

static void* _unmap(task_t *task, void *addr, int length, int flags) {
    length = (int)UPROUND(length, task->as.pgsize);
    for(int i = 0; i < task->adrnr; i++) {
        adrspc_t* space = task->adrlist[i];
        kmt->spin_lock(&space->adrlk);
        int size = (uintptr_t)space->area.end - (uintptr_t)space->area.start;
        if(addr == space->area.start && length == size) { // [addr, addr + len):检查是否是之前map出去的空间
            // Log("[%p, %p) #%d", space->area.start, space->area.end, task->id);
            if(space->share == 1) space->refcnt--; // 对于共享页面的unmap，只需将该地址段的引用计数减去 1 就可以了
            else for(int j = 0; j < space->pgnr; j++) dec_pgcnt(space->pa[j]); // 对于非共享页面的unmap，需要将该地址段的全部的页面的引用计数减少 1
            kmt->spin_unlock(&space->adrlk); 
            for(int j = i; j + 1 < task->adrnr; j++) task->adrlist[j] = task->adrlist[j + 1]; // 移除当前的地址段
            task->adrnr--;   
            return NULL; // 成功unmap
        }
        kmt->spin_unlock(&space->adrlk);    
    }
    panic("Should not reach here");
    return NULL;
}

static void* mmap(task_t *task, void *addr, int length, int prot, int flags) {
    switch(flags) {
        case MAP_SHARED: 
        case MAP_PRIVATE:
            return _mmap(task, addr, length, prot, flags);
        case MAP_UNMAP:
            return _unmap(task, addr, length, flags);
        default : panic("invalid flags");
    }
    return NULL;
}

static int getpid(task_t *task) {
	return task->id; 
}

static int sleep(task_t *task, int seconds) {
	uint64_t wakeup_time = io_read(AM_TIMER_UPTIME).us + 1000000L * seconds;
	while(io_read(AM_TIMER_UPTIME).us < wakeup_time) yield(); // 这里的上下文会被保存下来，当sleep的时间达到要求值时，将会返回到syscall函数
	return 0;
}

static int64_t uptime(task_t *task) {
    int64_t ret = io_read(AM_TIMER_UPTIME).us;
	return ret;
}

static Context* syscall(Event ev, Context* ctx) {
	task_t * proc = current_proc();
    assert(proc->ntrap == 1); // 嵌套必须是 1，系统调用前的上下文存在context[0]
	uintptr_t ret = 0;
	iset(true); // 进行系统调用前打开中断
    switch(ctx->GPRx) {
        case SYS_kputc: { kputc(NULL, ctx->GPR1); break; }
        case SYS_fork:  { ret = fork(proc); break; }
        case SYS_exit:  { ret = exit(proc, ctx->GPR1); break; }
        case SYS_wait:  { ret = wait(proc, (int *)ctx->GPR1); break; }
        case SYS_kill:  { ret = kill(NULL, ctx->GPR1); break; }
        case SYS_getpid:{ ret = getpid(proc); break; }
        case SYS_mmap:  { ret = (uintptr_t)mmap(proc, (void*)ctx->GPR1, ctx->GPR2, ctx->GPR3, ctx->GPR4); break; }
        case SYS_sleep: { sleep(NULL, ctx->GPR1); break; }
        case SYS_uptime:{ ret = uptime(NULL); break; }
        default :       { panic("Not implemented!"); }
    }
    assert(proc->ntrap == 1); // 嵌套必须是 0，之后进行schedule会将 1 减为 0 
    proc->context[0]->GPRx = ret; // 系统调用的最外层的上下文保存在context[0]上
    iset(false);
    proc->running = 0; // syscall如果内部yield的话，有可能将当前的任务running标记 1， 因此返回前需要将running标记为 0
    return NULL;
}

static void shr_pgmap(task_t * task, void * va, adrspc_t * space) {
    kmt->spin_lock(&space->adrlk); // 需要上锁
    void * pa = NULL;
    int i = 0, prot = space->prot;
    for(; i < space->pgnr; i++) {
        if(va == space->va[i]) { // 说明该共享页面已经被其他进程建立好映射了，此时直接map就可以了
            pa = space->pa[i];
            break;
        }
    }
    if(i == space->pgnr) { // 说明没有任何一个进程对该共享虚拟地址申请物理页面，那么我们需要申请新的物理页面
        pa = pmm->alloc(task->as.pgsize);
        space->va[space->pgnr] = va;
        space->pa[space->pgnr] = pa;
        space->pgnr++;
        panic_on(space->pgnr > PG_NR, "to many pages");
    }
    kmt->spin_unlock(&space->adrlk);
    map(&task->as, va, pa, prot); // map prot = share prot = true prot:  将共享页面添加到当前进程的地址空间
}

// 非共享页面的page map
static void unshr_pgmap(task_t* task, adrspc_t * space, void * va, void * pa, int prot) {
    int i = 0, pgnr = space->pgnr;
    AddrSpace * as = &task->as;
    for(; i < pgnr; i++) if(va == space->va[i]) break;
    space->va[i] = va;
    space->pa[i] = pa;
    if(i == pgnr) space->pgnr++; // 新进来的物理页面
    panic_on(space->pgnr > PG_NR, "to many pages");
    map(&task->as, va, pa, prot);
    Log("va = %p  pa = %p", va, pa);
    inc_pgcnt(pa); // to incre page cnt
    if((uintptr_t)va <= (uintptr_t)as->area.start + _init_len) { // code area 缺页时需要特殊处理
        int code_posi = (int)((uintptr_t)va - (uintptr_t)as->area.start); // 本次代码拷贝开始位置
        int copy_len = ((_init_len - code_posi) > as->pgsize) ? as->pgsize : (_init_len - code_posi); // 本次复制的长度
        memcpy(pa, _init + code_posi, copy_len);
    }
}

// 缺页处理函数：共享和非共享分开
static Context* pagefault(Event ev, Context* ctx) {
	task_t * proc = current_proc();
    AddrSpace* as = &proc->as;
    void * va = (void *)(ev.ref & ~(as->pgsize - 1L));
    int pgsize = as->pgsize;
    Log("va = %p  cause = %d  id = %d", va, ev.cause, proc->id);  // cause = 1, read;  cause = 2, write; cause = 3, read | write
    int tprot = -1, share = -1, i; // true prot, share
    adrspc_t * space = NULL;
    for(i = 0; i < proc->adrnr; i++) {
        space = proc->adrlist[i];
        Log("i = %d", i);
        kmt->spin_lock(&space->adrlk);
        if(IN_RANGE(va, space->area)) {
            tprot = space->prot; share = space->share;
            kmt->spin_unlock(&space->adrlk);    break;
        }
        kmt->spin_unlock(&space->adrlk);
    }
    Log("reach!");
    panic_on(tprot == -1 && share == -1, "invalid vaddr");
    if(share == 1) { // 共享页面处理
        shr_pgmap(proc, va, space);
        return NULL;
    }
    // 非共享页面的处理, 不需要上锁
    int maped = 0; // 地址段内是否已经有该虚拟地址的映射：yes, prot trans; no, pure page fault
    void * pa = NULL;
    for(i = 0; i < space->pgnr; i++) {
        if(va == space->va[i]) {
            maped = 1; pa = space->pa[i];
        }
    }
    if(maped == 0) {
        Log("pure page absence(unshared pages)"); 
        pa = pmm->alloc(pgsize);
        unshr_pgmap(proc, space, va, pa, PROT_READ); // 仅仅以只读的形式映射
    } else {
        Log("old page prot trans"); // 权限不足，此时需要将原有的页面引用计数改变
        panic_on(share == 1, "the prot of share page should not change"); 
        panic_on(!(tprot & PROT_WRITE) && (ev.cause & PROT_WRITE), "invalid prot"); // 检查真正的权限，如果原先不具备写权限但是现在要求写，那么出错
        map(as, va, pa, MMAP_NONE); // unmap，取消旧的物理页面的映射
        void * nwpa = pmm->alloc(as->pgsize); // 申请一个新的页面
        memcpy(nwpa, pa, as->pgsize); // 不要忘了将旧的页面的内容拷贝过来
        dec_pgcnt(pa); // 将原来的页面引用计数减去 1
        unshr_pgmap(proc, space, va, nwpa, tprot);
    }
    return NULL;
}

static adrspc_t* addralloc(void * start, void * end, int prot, int share) {
    adrspc_t* space = pmm->alloc(sizeof(adrspc_t));
    space->area.start = start; space->area.end = end;
    space->prot = prot;      space->share = share;
    kmt->spin_init(&space->adrlk, NULL);
    space->refcnt = 1;       space->pgnr = 0;
    return space;
}

static task_t * uproc_create(char * name, int runnable) {
    task_t * usr_task = pmm->alloc(sizeof(task_t)); // _init
    TASK_INIT(usr_task); // 初始化
    usr_task->name = name;
    usr_task->running = !(runnable); // 是否立即被调度
    protect(&usr_task->as);
    usr_task->context[0] = ucontext(&usr_task->as, (Area) {usr_task->stack, usr_task->stack + STACK_SIZE}, usr_task->as.area.start); // 第一个上下文初始化
    
    // 地址空间的第一个部分：代码区域
    usr_task->fraddr = usr_task->as.area.start + UPROUND(_init_len, usr_task->as.pgsize); // 用户mmap空闲空间起始地址:代码区域的终点
    adrspc_t* code = addralloc(usr_task->as.area.start, usr_task->fraddr,
                               PROT_READ | PROT_WRITE,  0);
    // 地址空间的第二个部分：用户栈
    adrspc_t* ustk = addralloc(usr_task->as.area.end - 1 * GB, usr_task->as.area.end,
                               PROT_READ | PROT_WRITE,  0); // 用户栈帧
    
    usr_task->adrlist[0] = code;
    usr_task->adrlist[1] = ustk;
    usr_task->adrnr = 2;    

    addTask(usr_task);
    return usr_task;
}
