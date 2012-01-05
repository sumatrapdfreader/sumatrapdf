#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>

extern void RunTests();

int main()
{
    RunTests();
    printf("All tests passed\n");
    return 0;
}

