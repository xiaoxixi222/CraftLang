#include <mcstd.h>
int g2 = 100;
void judge(int a, int b)
{
    print_int(a == b);
    print_int(a != b);
    print_int(a > b);
    print_int(a >= b);
    print_int(a < b);
    print_int(a <= b);
}
