#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>

static void crash_me()
{
    char *p = 0;
    *p = 0;
}

#define v(t) \
    if (!t) { \
        printf(#t); \
        crash_me(); \
    }

void RunTests()
{
    printf("Tests start\n");

    printf("Tests finished\n");
}

