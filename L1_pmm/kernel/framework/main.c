#include <kernel.h>
#include <klib.h>

int main() {
    os->init();
    // for (int i = 0; i < 7; i++) {
    //     size_t sz = 1 << 24;
    //     void *p = pmm->alloc(sz);
    //     printf("alloc %d bytes from pmm: %p\n", sz, p);
    //     assert(p != NULL);
    // }
    mpe_init(os->run);
    return 1;
}
