#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>

extern void RunTests();

int main()
{
    RunTests();
    printf("Console tests finished\n");
    return 0;
}

