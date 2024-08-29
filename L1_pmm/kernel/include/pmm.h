#ifndef _PMM_H
#define _PMM_H
#include <common.h>
#include <list.h>
#include <thread.h>

#define MAX_CACHES 10
#define PAGE_SIZE 4096
#define ALIGN(_A, _B) (((_A + _B - 1) / _B) * _B)
#define MIB (1024 * 1024)

// ==================== 以下是内存管理相关的数据结构 ====================

// ==================== 以下是slab分配器相关的数据结构 ====================
typedef struct object {
    struct object *next;
} object_t;

typedef struct slab {
    struct slab *next;        // 指向下一个slab
    object_t *free_objects;   // 指向空闲对象链表
    size_t num_free_objects;  // 空闲对象数
    lock_t slb_lock;          // 保护slab的锁
    size_t size;              // slab中对象的大小
} slab_t;

// slab分配器(链表数组)
typedef struct cache {
    slab_t *slabs;      // 指向slab链表
    size_t obj_size;    // 对象大小
    lock_t cache_lock;  // 保护cache的锁
} cache_t;

// ==================== 以下是伙伴系统相关的数据结构 ====================

struct free_list {
    struct list_head free_list;
    int nr_free;
};

/* 每一个block都是内存中连续的页面的集合 ：页面数量为2^order个 */
typedef struct buddy_block {
    struct list_head node;  // 用于空闲链表的节点
    size_t order;           // 页的阶数
    int free;               // block是否空闲
    int slab;               // slab分配器，不为空时，表示页面为slab分配器的一部分
} buddy_block_t;

// 伙伴系统的元数据
// [pool_start_addr, pool_end_addr)
typedef struct buddy_pool {
#define MIN_ORDER 0                              // 2^0 * 4KiB = 4 KiB
#define MAX_ORDER 12                             // 2^12 * 4KiB = 16 MiB
    struct free_list free_lists[MAX_ORDER + 1];  // 空闲链表(每一个链表对应一种类型的block)
    lock_t pool_lock[MAX_ORDER + 1];             // 保护伙伴系统的锁
    void *pool_meta_data;                        // 伙伴系统的元数据
    void *pool_start_addr;                       // 伙伴系统的起始地址
    void *pool_end_addr;                         // 伙伴系统的终止地址
} buddy_pool_t;

// ==================== 以下是buddy相关的函数 ====================
buddy_block_t *split2buddies(buddy_pool_t *pool, buddy_block_t *old, int new_order);
void *block2addr(buddy_pool_t *pool, buddy_block_t *block);
buddy_block_t *addr2block(buddy_pool_t *pool, void *addr);
void buddy_system_merge(buddy_pool_t *pool, buddy_block_t *block);
void *buddy_alloc(buddy_pool_t *pool, size_t size);
void buddy_free(buddy_pool_t *pool, void *ptr);
#endif /* _PMM_H */
