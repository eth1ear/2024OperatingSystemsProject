#include <thread.h>

#define TRUE 1
#define KiB *(1 << 10)
#define CPU_NR 8
#define SLAB_TYPE_NR 9
#define PGSZ (8 KiB) // 页面的大小
#define ALIGN(_A,_B) (((_A+_B-1)/_B)*_B)

typedef struct list{
	struct list *next; // 下一个header
	void * addr;
}list;

// 仅仅为了获取大小,无实际作用
typedef struct info{
	lock_t lk; // 当前页面的锁
	union page_t * next;
	int size; // 当前页面的size的类型
	list* header; // 存储数据区的链表的结构
}info;

typedef union page_t{
	struct {
		lock_t lk; // 当前页面的锁
		union page_t * next;
		int size; // 当前页面的size的类型
		list* header; // 存储数据区的链表的结构
	};
	uint8_t page[PGSZ];
} page_t;
