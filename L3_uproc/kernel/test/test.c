#include <common.h>
#include <stdio.h>
#include <am.h>
#include <thread.h>
#include <stdint.h>
#include <pmm.h>

#define PG_SZ 4096
#define MiB (1 << 20)
#define N 1030
#define TRUE 1
#define MAGIC 0x6666

struct op{
	void* addr;
	size_t size;
}queue[N];
int n , count = 0;
mutex_t lk = MUTEX_INIT();
cond_t cv = COND_INIT();
/*
测试框架：采用生产者消费者模型
*/
size_t alloc_random_sz() {
// 该函数的主要作用是返回一个随机大小的数值
  // int randint = rand() % 100000;
  // if(randint == 0) {
  // 	debug("Big Size Test!\n");
  // 	// assert(0);
  //   return  (rand() % 5 + 1) * MiB;
  // }
  int randint = rand() % 100;
  if(randint == 0) {
  	// 1 % 的概率
    // return  (rand() % 15 + 1) * MiB;
    return PG_SZ; // 返回若干个页面的大小
  } else if(randint <= 5){
  	// 10 % 的概率
    // 返回 [129 ,4096] 中的随意值
    return (rand() % (4096 - 128)) + 129;
  } else {
  	// 89 % 的概率
		// 测试[1, 128]B的申请
  	return (rand() % 128) + 1;
  }
}

// 检查对齐与否
void alignment_check(void *ptr, size_t sz) {
  if(sz == 1)return ;
  for(int i = 1; i < 64; i++) {
    if(sz > (1 << (i - 1))&&
       sz <= (1 << i)
      )
      {
        if(((uintptr_t)ptr % (1 << i)) != 0) {
        debug("Alignment is not satisfied! size = %ld, addr = %p\n", sz, ptr);
        assert(0);
        } else {
          return ;
        }
      }
  }
}


void double_alloc_check(void *ptr, size_t size) {
  unsigned int * arr = (unsigned int *)ptr;
  for (int i = 0; (i + 1) * sizeof(unsigned int) <= size; i++) {
  	if(arr[i] == MAGIC) {
			debug("Double Alloc at addr %p!\n", ptr); assert(0);
  	}
	  arr[i] = MAGIC;
	}
	// debug("Pass double_alloc_check %d!\n", size);
}

// 注意不应该将malloc的调用放在锁内部，否则不同线程无法并发地调用malloc，也就是每次都只有一个线程进行malloc的申请
// 需要确保每一次从队列里面添加的都是不同的申请对象，alloc需要确保即使在并发的情况下依旧能够返回正确的空间
void Tproduce(int id) {
	while(1) {
		sleep(0.1); // 防止每一个线程的申请频率过高
		// 创建一个新的allocation
		size_t sz = alloc_random_sz();
		debug("%d th Producer trys to alloc %ld\n", id, sz);
		void * ret = pmm->alloc(sz);
		assert(ret); 
		// debug("Return from alloc successfully\n");
		double_alloc_check(ret, sz);
		mutex_lock(&lk);
		while(count == n) {
			cond_wait(&cv, &lk);
		}
		count++;
		queue[count].addr = ret;
		queue[count].size = sz;
		cond_broadcast(&cv);
		mutex_unlock(&lk);
		alignment_check(ret, sz);
	}
}

void Tconsume(int id) {
	while(1){
		mutex_lock(&lk);
		while(count == 0) {
			cond_wait(&cv, &lk);
		}
		// free 
		void* ptr = queue[count].addr;
		size_t size = queue[count].size;
    count--;
		cond_broadcast(&cv);
		mutex_unlock(&lk);
		// 上面的代码确保每一个消费者从队列里面取出来的都是不同的free对象
		unsigned int * arr = (unsigned int *)ptr;
	  for (int i = 0; (i + 1) * sizeof(unsigned int) <= size; i++) {
		  arr[i] = 0; // 清零
		}
		assert(ptr);
		debug("%d th Consumer trys to free %p\n", id, ptr);
		pmm->free(ptr);
		sleep(0.1); // 防止free的频率过高
	}

}
int main() {
	n = 1024;
  pmm->init();
	for(int i = 0; i < 8; i++) {
		create(Tproduce);
		create(Tconsume);
	}
}

