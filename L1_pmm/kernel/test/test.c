
#include "common.h"
#include "thread.h"

static inline size_t min(size_t a, size_t b) { return a < b ? a : b; }
#define N 100010
#define PG_SZ 4096
#define MAGIC 0x6666

struct op {
    void* addr;
    size_t size;
} queue[N];

int n, count = 0;              // 缓冲区的大小，以及当前缓冲区的元素数量
#define MAX_MEM_SZ (96 << 20)  // 96MB

size_t total_sz = 0;
mutex_t lk = MUTEX_INIT();
cond_t cv = COND_INIT();

// ==================================
// 通用函数

/**
 * @brief 生成一个随机的数:
 * 1%的概率返回若页面倍数的大小;
 * 10%的概率返回[129B, 4096B]中的随机值;
 * 89%的概率返回[1B, 128B]中的随机值;
 */
size_t alloc_random_sz() {
    int percent = rand() % 100;
    if (percent == 0) {
        return (rand() % 1024 + 1) * PG_SZ;
    } else if (percent <= 10) {
        return (rand() % (4096 - 128)) + 129;
    } else {
        return rand() % 128 + 1;
    }
}

/**
 * @brief 返回[1, 2048]中的随机值： 85的概率为[1, 128], 15的概率为[129, 2048]
 */
size_t small_random_sz() {
    int percent = rand() % 100;
    if (percent < 85) {
        return rand() % 128 + 1;
    } else {
        return (rand() % (2048 - 128)) + 129;
    }
}

size_t pg_sz() { return PG_SZ; }

/**
 * @brief 检查一个地址是否对齐到sz大小
 * @param ptr 要检查的地址
 * @param sz 对齐的大小
 */
void alignment_check(void* ptr, size_t sz) {
    if (sz == 1) return;
    for (int i = 1; i < 64; i++) {
        if (sz > (1 << (i - 1)) && sz <= (1 << i)) {
            if (((uintptr_t)ptr % (1 << i)) != 0) {
                PANIC("Alignment is not satisfied! size = %ld, addr = %p\n", sz, ptr);
            } else {
                return;
            }
        }
    }
}

/**
 * @brief 检查一个地址是否被二次分配:核心方法时通过magic number检查
 * @param ptr 要检查的地址
 * @param size 要检查的大小
 */
void double_alloc_check(void* ptr, size_t size) {
    unsigned int* arr = (unsigned int*)ptr;
    for (int i = 0; (i + 1) * sizeof(unsigned int) <= size; i++) {
        if (arr[i] == MAGIC) {
            PANIC("Double Alloc at addr %p with size %ld!\n", ptr, size);
        }
        arr[i] = MAGIC;
    }
}

/**
 * @brief 清除一个地址的magic number：置 0
 */
void clear_magic(void* ptr, size_t size) {
    unsigned int* arr = (unsigned int*)ptr;
    for (int i = 0; (i + 1) * sizeof(unsigned int) <= size; i++) {
        arr[i] = 0;
    }
}

/**
 * @brief 带有循环次数限制的producer，测试slab分配器
 */
void loop_small_sz_producer(int id) {
    n = (64 << 10);  // 64K
    for (int i = 0; i < n; i++) {
        size_t sz = small_random_sz();
        // debug("%d th Producer trys to alloc %ld\n", id, sz);
        void* ret = pmm->alloc(sz);
        PANIC_ON(ret == NULL, "pmm->alloc(%ld) failed!\n", sz);
        double_alloc_check(ret, sz);
        alignment_check(ret, sz);

        // 将分配的地址和大小放入队列中，如果队列满了则等待
        mutex_lock(&lk);
        while (count == n) {
            cond_wait(&cv, &lk);
        }

        // 将分配的地址和大小放入队列中，并通知消费者，消费者负责进行free
        count++;
        assert(count <= n);
        queue[count].addr = ret;
        queue[count].size = sz;
        cond_broadcast(&cv);
        mutex_unlock(&lk);
    }
}

/**
 * @brief 带有循环次数限制的consumer
 */
void loop_small_sz_consumer(int id) {
    n = (64 << 10);
    for (int i = 0; i < n; i++) {
        // 从队列中取出一个地址和大小，如果队列为空则等待
        mutex_lock(&lk);
        while (count == 0) {
            cond_wait(&cv, &lk);
        }

        // 从队列中取出一个地址和大小，并通知生产者，生产者负责进行alloc
        // 随机的取出一个队列的元素
        int index = rand() % count + 1;
        void* addr = queue[index].addr;
        size_t sz = queue[index].size;
        // 队列最后一个元素拷贝到当前位置，free_index(count)--
        queue[index].addr = queue[count].addr;
        queue[index].size = queue[count].size;
        count--;
        assert(count >= 0);
        cond_broadcast(&cv);
        mutex_unlock(&lk);
        clear_magic(addr, sz);
        pmm->free(addr);
        // sleep(0.1);
    }  // 清除magic number
}

// ==================================

/**
 * @brief 单线程检查申请到的内存是否对齐到2^k
 */
void do_test0() {
    printf(
        "\033[44mTest 0: Check if all the return sizes are to the power of "
        "2\033[0m\n");
    for (int i = 0; i < 100000; i++) {
        int sz = alloc_random_sz() % 2048 + 1;  // [1, 2048]B的内存申请
        void* ret = pmm->alloc(sz);
        double_alloc_check(ret, sz);
        alignment_check(ret, sz);
        pmm->free(ret);
        clear_magic(ret, sz);
    }
}

/**
 * @brief 只有一个生产者和一个消费者(loop 10000 times each),测试slab分配器
 */
void do_test1() {
    printf(
        "\033[44mTest 1: Check if the producer(#=1) and consumer(#=1) can work "
        "correctly(for small size alloc)\033[0m\n");
    create(loop_small_sz_producer);
    create(loop_small_sz_consumer);
    join();
}

/**
 * @brief 8个生产者和8个消费者(loop 100000 times each)(测试slab分配器)
 */
void do_test2() {
    printf(
        "\033[45mTest 2: Check if the producer(#=8) and consumer(#=8) can work "
        "correctly(for small size alloc)\033[0m\n");
    for (int i = 1; i <= 8; i++) {
        create(loop_small_sz_producer);
        create(loop_small_sz_consumer);
    }
    join();
}

void pages_producer(int id) {
    n = (16 << 10);  // 16K
    for (int i = 0; i < n; i++) {
        size_t sz = 4096 * (rand() % 128 + 1);  // 页面大小的随机倍数
        sz = min(sz, (64 << 20) - total_sz);
        void* ret = pmm->alloc(sz);
        PANIC_ON(ret == NULL, "pmm->alloc(%ld) failed!\n", sz);
        double_alloc_check(ret, sz);
        alignment_check(ret, sz);

        // 将分配的地址和大小放入队列中，如果队列满了则等待
        mutex_lock(&lk);
        total_sz += sz;
        while (count == n) {
            cond_wait(&cv, &lk);
        }

        // 将分配的地址和大小放入队列中，并通知消费者，消费者负责 进行free
        count++;
        assert(count <= n);
        queue[count].addr = ret;
        queue[count].size = sz;
        cond_broadcast(&cv);
        mutex_unlock(&lk);
    }
}

void pages_consumer(int id) {
    n = (16 << 10);  // 16K
    for (int i = 0; i < n; i++) {
        // 从队列中取出一个地址和大小，如果队列为空则等待
        mutex_lock(&lk);
        while (count == 0) {
            cond_wait(&cv, &lk);
        }

        // 从队列中取出一个地址和大小，并通知生产者，生产者负责进行alloc
        void* addr = queue[count].addr;
        size_t sz = queue[count].size;
        total_sz -= sz;
        count--;
        assert(count >= 0);
        cond_broadcast(&cv);
        mutex_unlock(&lk);
        clear_magic(addr, sz);
        pmm->free(addr);
    }  // 清除magic number
}

/**
 * @brief 单线程的页面大小的申请和释放
 */
void do_test3() {
    printf("\033[45mTest 3: exact page alloc and free in a single thread\033[0m\n");
    for (int i = 0; i < 2000; i++) {
        int sz = PG_SZ;
        void* ret = pmm->alloc(sz);
        double_alloc_check(ret, sz);
        alignment_check(ret, sz);
        pmm->free(ret);
        clear_magic(ret, sz);
    }
}

/**
 * @brief 一个producer和一个consumer，页面倍数大小的申请和释放
 */
void do_test4() {
    printf("\033[44mTest 4: pages alloc and free\033[0m\n");
    create(pages_producer);
    create(pages_consumer);
    join();
}

/**
 * @brief 页面倍数大小的申请和释放(#8 producer & #8 consumer)
 */
void do_test5() {
    printf("\033[44mTest 5: pages alloc and free in multi thread(#8 producer & #8 consumer)\033[0m\n");
    for (int i = 0; i < 8; i++) {
        create(pages_producer);
        create(pages_consumer);
    }
    join();
}

/**
 * 返回一个大内存申请的大小，1/3的概率返回(8, 10] MIB，1/3的概率返回(10, 12] MIB，1/3的概率返回(12, 16] MIB
 */
size_t big_sz() {
    int i = rand() % 3;
    size_t sz = 0;
    switch (i) {
        case 0:
            // 随机生成(8, 10] MIB的sz
            sz = rand() % (2 * 1024 * 1024) + 8 * 1024 * 1024;
            break;
        case 1:
            // 随机生成(10, 12] MIB的sz
            sz = rand() % (2 * 1024 * 1024) + 10 * 1024 * 1024;
            break;
        case 2:
            // 随机生成(12, 16] MIB的sz
            sz = rand() % (4 * 1024 * 1024) + 12 * 1024 * 1024;
            break;
        default:
            break;
    }
    // debug("try to alloc %ldMiB big sz\n", sz / (1 << 20));
    return sz;
}

/**
 * 以1/4000的概率返回big_sz
 */
size_t combined_sz() {
    int i = rand() % 4096;
    if (i == 0) {
        return big_sz();
    } else if (i <= 3) {
        // 返回[1MiB, 8MiB)的随机数
        size_t sz = rand() % (7 * 1024 * 1024) + 1024 * 1024;
        // debug("try to alloc %ldMiB small sz\n", sz / (1 << 20));
        return sz;
    } else {
        return alloc_random_sz();
    }
}

void combined_producer() {
    n = (64 << 10);
    for (int i = 0; i < n; i++) {
        size_t sz = combined_sz();
        // debug("try to alloc sz = %ld\n", sz);
        void* ret = pmm->alloc(sz);
        PANIC_ON(ret == NULL, "pmm->alloc(%ld) failed!\n", sz);
        double_alloc_check(ret, sz);
        alignment_check(ret, sz);

        // 将分配的地址和大小放入队列中，如果队列满了则等待
        mutex_lock(&lk);
        while (count == n) {
            cond_wait(&cv, &lk);
        }

        // 将分配的地址和大小放入队列中，并通知消费者，消费者负责 进行free
        count++;
        assert(count <= n);
        queue[count].addr = ret;
        queue[count].size = sz;
        cond_broadcast(&cv);
        mutex_unlock(&lk);
    }
}

void combined_consumer() {
    n = (64 << 10);
    for (int i = 0; i < n; i++) {
        // 从队列中取出一个地址和大小，如果队列为空则等待
        mutex_lock(&lk);
        while (count == 0) {
            cond_wait(&cv, &lk);
        }

        // 从队列中取出一个地址和大小，并通知生产者，生产者负责进行alloc
        void* addr = queue[count].addr;
        size_t sz = queue[count].size;
        count--;
        assert(count >= 0);
        cond_broadcast(&cv);
        mutex_unlock(&lk);
        clear_magic(addr, sz);
        pmm->free(addr);
    }  // 清除magic number
}

/**
 * @brief 混合大小测试(#8 producer & #8 consumer)
 */
void do_test6() {
    printf("\033[44mTest 6: mixed alloc and free using multi-thread\033[0m\n");
    for (int i = 0; i < 8; i++) {
        create(combined_producer);
        create(combined_consumer);
    }
    join();
}

int main(int argc, char* argv[]) {
    printf("\033[32mBegin Using our Testing Framework!\033[0m\n");
    if (argc < 2) exit(1);
    pmm->init();
    switch (atoi(argv[1])) {
        case 0:
            do_test0();
            break;
        case 1:
            do_test1();
            break;
        case 2:
            do_test2();
            break;
        case 3:
            do_test3();
            break;
        case 4:
            do_test4();
            break;
        case 5:
            do_test5();
            break;
        case 6:
            do_test6();
            break;
        default:
            PANIC("No Test Case!");
    }

    // 通过对应的测试样例，打印绿色的通过字体

    printf("\033[42mTest %d Passed!\033[0m\n", atoi(argv[1]));
}
