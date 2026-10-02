#include <mcstd.h>

int g1 = 10;
extern int g2;

int add1(int a);
int add2(int a);

int main()
{
    print_int(add1(g1));
    print_int(add2(g2));
    return 0;
}
