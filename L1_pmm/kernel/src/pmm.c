/**
 * File Name: pmm.c
 * Author: Xuqin Zhang
 * Update Date: 2023-8-26
 */
#include <common.h>
#include <pmm.h>

// initialize in pmm_init()
static void *pmm_end = NULL;
static void *pmm_start = NULL;

// ==================== Common Function ====================
// Align size to 2^N
static size_t align_size(size_t size) {
  size_t ret = 1;
  while (ret < size) ret <<= 1;
  ret = (ret > 8) ? ret : 8;
  return ret;
}

// ==================== (1) Buddy System related ====================

#define PAGE_SHIFT 12
static size_t buddy_mem_sz = 0;
static buddy_pool_t g_buddy_pool = {};
static lock_t global_lock = LOCK_INIT();

/// @brief size is at least 1 page, return the order of the size, order is at
/// least 0
static inline size_t buddy_block_order(size_t size) {
  size_t order = 0;
  while ((1 << order) < size) {
    order++;
  }
  return order;
}

/// @brief simply for debugging
/// @param pool buddy pool
void print_pool(buddy_pool_t *pool) {
  for (int i = 0; i <= MAX_ORDER; i++) {
    struct list_head *list = &(pool->free_lists[i].free_list);
    if (list_empty(list)) {
      continue;
    }
    debug("order %d:\n ", i);
    buddy_block_t *block = (buddy_block_t *)list->next;
    while (&block->node != list) {
      debug(
          "%p 对应的地址范围是[%ld, %ld)\n", block,
          (uintptr_t)block2addr(pool, block),
          (uintptr_t)block2addr(pool, block) + (1 << block->order) * PAGE_SIZE);
      block = (buddy_block_t *)block->node.next;
    }
    debug("\n");
  }
}
/**
 * @brief Initialize buddy system
 * Memory layout:
 * [start, meta_data_size + start) stores the meta data of buddy system
 * [meta_data_size + start, end) stores the memory that can be allocated
 * @param start memory start address
 * @param end memory end address
 * @note
 */
void buddy_pool_init(buddy_pool_t *pool, void *start, void *end) {
  // 1. initialize the free lists
  size_t page_num = (end - start) >> PAGE_SHIFT;
  pool->pool_meta_data = start;
  debug("buddy pool init: start = %p, end = %p, page_num = %ld\n", start, end,
        page_num);
  for (int i = 0; i <= MAX_ORDER; i++) {
    init_list_head(&(pool->free_lists[i].free_list));
  }

  debug("meta data of buddy system [%p, %p)\n", pool->pool_meta_data,
        pool->pool_meta_data + page_num * sizeof(buddy_block_t));

  memset((char *)(pool->pool_meta_data), 0, page_num * sizeof(buddy_block_t));
  start += page_num * sizeof(buddy_block_t);
  page_num -= page_num * sizeof(buddy_block_t) >> PAGE_SHIFT;
  pool->pool_start_addr = (void *)ALIGN(((uintptr_t)start), PAGE_SIZE);
  pool->pool_end_addr = end;
  page_num = (pool->pool_end_addr - pool->pool_start_addr) >> PAGE_SHIFT;
  debug("memory that can be allocated [%p, %p)\n", pool->pool_start_addr,
        pool->pool_end_addr);

  // 将整个内存空间分成一个个page，每个page都绑定一个buddy_block_t
  int page_idx;
  for (page_idx = 0; page_idx < page_num; page_idx++) {
    buddy_block_t *block = (buddy_block_t *)(pool->pool_meta_data +
                                             sizeof(buddy_block_t) * page_idx);
    block->order = 0;
    block->free = 0;
  }

  for (page_idx = 0; page_idx < page_num; page_idx++) {
    buddy_block_t *block = (buddy_block_t *)(pool->pool_meta_data +
                                             sizeof(buddy_block_t) * page_idx);
    void *addr = block2addr(pool, block);
    buddy_free(pool, addr);
  }

  // print_pool(pool);
}

// split the block until the order of the block is equal to target_order
buddy_block_t *buddy_system_split(buddy_pool_t *pool, buddy_block_t *block,
                                  int target_order) {
  buddy_block_t *ret = block;
  int order = block->order;
  while (order > 0 && order >= target_order + 1) {
    order--;
    ret = split2buddies(pool, ret, order);
  }
  return ret;
}

// get the buddy chunk of the block
buddy_block_t *get_buddy_chunk(buddy_pool_t *pool, buddy_block_t *block) {
  uintptr_t addr = (uintptr_t)block2addr(pool, block);
  uintptr_t buddy_addr = addr ^ (1UL << (block->order + PAGE_SHIFT));
  if (buddy_addr < (uintptr_t)pool->pool_start_addr ||
      buddy_addr + (1UL << (block->order + PAGE_SHIFT)) >=
          (uintptr_t)pool->pool_end_addr) {
    return NULL;
  }
  return addr2block(pool, (void *)buddy_addr);
}

// split the block into two buddies
buddy_block_t *split2buddies(buddy_pool_t *pool, buddy_block_t *old,
                             int new_order) {
  PANIC_ON(old->order <= 0, "split2buddies");
  uintptr_t left_addr = (uintptr_t)block2addr(pool, old);
  uintptr_t right_addr = left_addr + (1UL << (new_order + PAGE_SHIFT));
  buddy_block_t *left = addr2block(pool, (void *)left_addr);
  buddy_block_t *right = addr2block(pool, (void *)right_addr);
  left->order = right->order = new_order;
  left->free = 0;
  right->free = 1;
  list_add((struct list_head *)right, &(pool->free_lists[new_order].free_list));
  pool->free_lists[new_order].nr_free++;
  return left;
}

// convert block to address
void *block2addr(buddy_pool_t *pool, buddy_block_t *block) {
  int index = ((void *)block - pool->pool_meta_data) / sizeof(buddy_block_t);
  void *addr = index * PAGE_SIZE + pool->pool_start_addr;
  return addr;
}

// convert address to block
buddy_block_t *addr2block(buddy_pool_t *pool, void *addr) {
  PANIC_ON(((uintptr_t)addr % PAGE_SIZE),
           "addr is supposed to aligned to page");
  int index = (uintptr_t)(addr - pool->pool_start_addr) >> PAGE_SHIFT;
  return (buddy_block_t *)(pool->pool_meta_data +
                           index * sizeof(buddy_block_t));
}

// merge the block with its buddy until the order of the block is equal to the
// order of its buddy
void buddy_system_merge(buddy_pool_t *pool, buddy_block_t *block) {
  int order = block->order;
  while (order < MAX_ORDER) {
    buddy_block_t *buddy = get_buddy_chunk(pool, block);
    if (buddy == NULL || buddy->free == 0 || buddy->order != order) {
      break;
    }
    list_del(&(buddy->node));  // 将buddy从其所在的list中删除
    pool->free_lists[order].nr_free--;
    if ((uintptr_t)block > (uintptr_t)buddy) block = buddy;
    order++;
    block->order = order;
    block->free = 1;
  }
  block->order = order;
  block->free = 1;
  list_add(&(block->node), &(pool->free_lists[order].free_list));
  pool->free_lists[order].nr_free++;
}

// allocate a chunk from buddy system
void *buddy_alloc(buddy_pool_t *pool, size_t size) {
  lock(&global_lock);
  size = align_size(size);
  int order = buddy_block_order(size >> PAGE_SHIFT);
  buddy_block_t *block = NULL;
  for (int i = order; i <= MAX_ORDER; i++) {
    struct list_head *list = &(pool->free_lists[i].free_list);
    if (list_empty(list)) {
      continue;
    }
    block = (buddy_block_t *)list->next;
    list_del((struct list_head *)block);
    pool->free_lists[i].nr_free--;
    block->free = 0;
    block = buddy_system_split(pool, block, order);
    break;
  }
  if (block == NULL) {
    return NULL;
  }
  unlock(&global_lock);
  return block2addr(pool, block);
}

// free a chunk to buddy system
void buddy_free(buddy_pool_t *pool, void *ptr) {
  lock(&global_lock);
  buddy_block_t *block = addr2block(pool, ptr);
  buddy_system_merge(pool, block);
  unlock(&global_lock);
}

// ==================== (2) Slab allocator related ====================

/**
 * typedef struct cache {
    slab_t *slabs;      // linked list of slabs
    size_t obj_size;    // size of object in this cache, 8, 16, 32, ...2048
    lock_t cache_lock;  // lock for cache
} cache_t;
*/
static cache_t g_caches[MAX_CACHES];  // global cache manager, MAX_CACHES = 9

// initialize g_caches, obj_size = 8, 16, 32, ...2048
void slab_allocator_init() {
  size_t obj_sz = 8;
  for (int i = 0; i < MAX_CACHES; i++) {
    g_caches[i].slabs = NULL;
    g_caches[i].obj_size = obj_sz;
    g_caches[i].cache_lock = LOCK_INIT();
    obj_sz *= 2;
  }
}

// find the cache that can allocate an object of size(the first cache that
// obj_size >= size)
static cache_t *find_cache(size_t size) {
  for (int i = 0; i < MAX_CACHES; i++) {
    if (g_caches[i].obj_size >= size) {
      return &g_caches[i];
    }
  }
  return NULL;
}

// allocate a slab and add it to the cache
static slab_t *allocate_slab(cache_t *cache) {
  /**
   * 1. allocate a page from buddy system as the start pointer of the slab
   * 2. fill the slab with meta data and then objects(see object_t in pmm.h),
   * objects are linked by a linked list
   * 3. add the slab to the cache
   */
  uintptr_t slab_addr = (uintptr_t)buddy_alloc(&g_buddy_pool, PAGE_SIZE);
  buddy_block_t *block = addr2block(&g_buddy_pool, (void *)slab_addr);
  block->slab = 1;
  assert(slab_addr % PAGE_SIZE == 0);

  slab_t *new_slab = (slab_t *)slab_addr;

  slab_addr += sizeof(slab_t);
  slab_addr = ALIGN(slab_addr, cache->obj_size);

  size_t num_obj =
      (PAGE_SIZE - (slab_addr - (uintptr_t)new_slab)) / cache->obj_size;

  object_t *obj = (object_t *)slab_addr;
  new_slab->free_objects = obj;
  new_slab->num_free_objects = num_obj;
  new_slab->size = cache->obj_size;
  new_slab->slb_lock = LOCK_INIT();

  // fill the slab with objects, and link them by a linked list, all these
  // objects are aligned to 2^(obj_size)
  for (int i = 0; i < num_obj - 1; i++) {
    obj->next = (object_t *)((uintptr_t)obj + new_slab->size);
    obj = obj->next;
    assert((uintptr_t)obj + new_slab->size <= (uintptr_t)new_slab + PAGE_SIZE);
  }
  obj->next = NULL;

  return new_slab;
}

/**
 * allocate an object from the cache, if there is no free object, allocate a new
 * slab and then allocate an object from the new slab, this function is
 * thread-safe
 */
void *slab_alloc(size_t size) {
  if (size == 0 || size >= PAGE_SIZE) {
    return NULL;
  }

  cache_t *cache = find_cache(size);
  if (cache == NULL) {
    PANIC("slab alloc");
    return NULL;
  }

  slab_t *slab = cache->slabs;
  while (slab != NULL) {
    lock(&slab->slb_lock);
    if (slab->num_free_objects > 0) {
      object_t *obj = slab->free_objects;
      slab->free_objects = obj->next;
      slab->num_free_objects--;
      unlock(&slab->slb_lock);
      return obj;
    } else {
      unlock(&slab->slb_lock);
      slab = slab->next;
    }
  }
  assert(slab == NULL);

  slab = allocate_slab(cache);
  lock(&slab->slb_lock);
  object_t *obj = slab->free_objects;
  slab->free_objects = obj->next;
  slab->num_free_objects--;
  unlock(&slab->slb_lock);
  lock(&cache->cache_lock);
  slab->next = cache->slabs;
  cache->slabs = slab;
  unlock(&cache->cache_lock);

  return obj;
}

/**
 * free an object, this function is thread-safe
 */
void slab_free(void *ptr) {
  /**
   * 实现核心： ptr 低12位全部清0 -> Slab的开始地址（存储元信息的结构体）
   */
  if (ptr == NULL) {
    return;
  }
  object_t *obj = (object_t *)ptr;
  slab_t *slab = (slab_t *)((uintptr_t)ptr & ~(PAGE_SIZE - 1));  // 低12位清零

  lock(&slab->slb_lock);
  obj->next = slab->free_objects;
  slab->free_objects = obj;
  slab->num_free_objects++;
  unlock(&slab->slb_lock);
}

//======================= (3) Functions to outside ========================

static void *kalloc(size_t size) {
  void *ret = NULL;
  size = align_size(size);
  if (size > (1 << MAX_ORDER) * PAGE_SIZE) {  // 不能分配超过最大order的内存
    ret = NULL;
  } else if (size >= PAGE_SIZE) {
    ret = buddy_alloc(&g_buddy_pool, size);
    PANIC_ON(((uintptr_t)ret >= (uintptr_t)g_buddy_pool.pool_end_addr),
             "buddy_alloc failed");
  } else {
    ret = slab_alloc(size);
  }
  return ret;
}

static void kfree(void *ptr) {
  void *page = (void *)((uintptr_t)ptr & ~(PAGE_SIZE - 1));
  buddy_block_t *block = addr2block(&g_buddy_pool, page);
  if (block->slab) {
    slab_free(ptr);
  } else {
    buddy_free(&g_buddy_pool, ptr);
  }
}

#ifndef TEST
static void pmm_init() {
  heap.start = (void *)ALIGN((uintptr_t)(heap.start), PAGE_SIZE);
  heap.end = (void *)ALIGN((uintptr_t)(heap.end), PAGE_SIZE);
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  pmm_start = heap.start;
  pmm_end = heap.end;
  buddy_mem_sz = pmsize;
  debug("pmm_start = %p, pmm_end = %p, buddy_mem_sz = %d\n", pmm_start, pmm_end,
        buddy_mem_sz);
  buddy_pool_init(&g_buddy_pool, pmm_start, pmm_end);
  slab_allocator_init();
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
}
#else
// 我们测试框架中的pmm_init()函数
#define HEAP_SIZE (512 << 20)
Area heap = {};
static void pmm_init() {
  char *ptr = malloc(HEAP_SIZE);
  PANIC_ON(ptr == NULL, "pmm init malloc failed");
  heap.start = ptr;
  heap.end = ptr + HEAP_SIZE;
  heap.start = (void *)ALIGN((uintptr_t)(heap.start), PAGE_SIZE);
  heap.end = (void *)ALIGN((uintptr_t)(heap.end), PAGE_SIZE);
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  pmm_start = heap.start;
  pmm_end = heap.end;  // 这两个量之后将会保持恒定
  buddy_mem_sz = pmsize;
  debug("pmm_start = %p, pmm_end = %p, buddy_mem_sz = %ld\n", pmm_start,
        pmm_end, buddy_mem_sz);
  buddy_pool_init(&g_buddy_pool, pmm_start, pmm_end);
  slab_allocator_init();
  printf("Got %ld MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);
}
#endif

MODULE_DEF(pmm) = {
    .init = pmm_init,
    .alloc = kalloc,
    .free = kfree,
};
