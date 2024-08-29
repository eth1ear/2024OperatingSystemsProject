#define MAP_SHARED    1 // 读权限
#define MAP_PRIVATE   2 // 写权限
#define MAP_UNMAP     3

#define PROT_NONE   0x0 // 0x1
#define PROT_READ   0x1 // 0x2
#define PROT_WRITE  0x2 // 0x4

#define MMAP_NONE  0x00000000 // no access
#define MMAP_READ  0x00000001 // can read
#define MMAP_WRITE 0x00000002 // can write

#define UPROUND(_A,_B) (((_A+_B-1)/_B)*_B) // 将_A向上对齐到B
// #define IN_RANGE(ptr, area) ((area).start <= (ptr) && (ptr) < (area).end)
// struct phypage {
// 	void *pa;
// 	int  refcnt; // 相关引用计数
// }pa_t;	
