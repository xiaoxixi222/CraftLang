#include <mcstd.h>

int g = 1;

int scope_test(int p)
{
    int g = 10;
    print_int(g); // 10 局部遮蔽全局
    {
        int g = 100;
        print_int(g); // 100 内层块遮蔽外层
    }
    print_int(g); // 10 块结束后应恢复外层 g
    if (p > 0)
    {
        int inner = p + 1;
        print_int(inner); // inner 仅在 if 块内可见
    }
    print_int(g);
    return g + p;
}
