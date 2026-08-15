#include <mcstd.h>

int g1 = 10;
extern int g2;

int add(int a, int b)
{
    return a + b;
}

int sub(int a, int b)
{
    return a - b;
}

int mul(int a, int b)
{
    return a * b;
}

int divi(int a, int b)
{
    return a / b;
}

int mod(int a, int b)
{
    return a % b;
}

int main()
{
    int a = 10, b = 20;
    int c = add(a, b);
    int d = sub(b, a);
    int e = mul(a, 2);
    int f = divi(b, a);
    int m = mod(b, 3);
    int p = (a + b) * 2;
    int n = -a;
    c = c + 1;
    g1 = g1 + c;
    c += 5;
    c -= 2;
    c *= 3;
    c /= 11;
    c %= 4;
    g2 += 50;
    print_int(c);
    print_int(d);
    print_int(e);
    print_int(f);
    print_int(m);
    print_int(p);
    print_int(n);
    print_int(g1);
    print_int(g2);
    print_int(c);
    return 0;
}
