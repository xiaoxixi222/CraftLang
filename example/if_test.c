#include <mcstd.h>

int classify(int x)
{
    if (x > 0)
    {
        return 1;
    }
    else if (x < 0)
    {
        return -1;
    }
    else
    {
        if (x == 0)
        {
            return 0;
        }
    }
    return -100;
}