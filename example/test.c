#include <mcstd.h>

int add(int a, int b);

int d = 0;
int main()
{
    int a = 10, b = 20;
    int c = add(a, b);
    print_int(c);
    return 0;
}

int add(int a, int b)
{
    return a + b * b;
}