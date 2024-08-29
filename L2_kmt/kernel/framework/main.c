#include <kernel.h>
#include <klib.h>

int main() {
    ioe_init();
    cte_init(os->trap);
    os->init();
    // 在这之前一直都只有一个处理器在执行，编号为 0 的CPU
    mpe_init(os->run);
    return 1;
}