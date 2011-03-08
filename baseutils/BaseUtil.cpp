/* Written by Krzysztof Kowalczyk (http://blog.kowalczyk.info)
   The author disclaims copyright to this source code. */
#include "BaseUtil.h"

void swap_int(int *one, int *two)
{
    int tmp = *one;
    *one = *two;
    *two = tmp;
}

void swap_double(double *one, double *two)
{
    double tmp = *one;
    *one = *two;
    *two = tmp;
}

void *memdup(void *data, size_t len)
{
    void *dup = malloc(len);
    if (dup)
        memcpy(dup, data, len);
    return dup;
}
