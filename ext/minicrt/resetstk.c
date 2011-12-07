/***
*resetstk.c - Recover from Stack overflow.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the _resetstkoflw() function.
*
*******************************************************************************/

#include <stdlib.h>
#include <malloc.h>
#include <windows.h>

#define MIN_STACK_REQ_WIN9X 17
#define MIN_STACK_REQ_WINNT 2

#define _osplatform VER_PLATFORM_WIN32_NT

/***
* void _resetstkoflw() - Recovers from Stack Overflow
*
* Purpose:
*       Sets the guard page to its position before the stack overflow.
*
* Exit:
*       Returns nonzero on success, zero on failure
*
*******************************************************************************/

int _resetstkoflw()
{
    LPBYTE pStack, pGuard, pStackBase, pMaxGuard, pMinGuard;
    MEMORY_BASIC_INFORMATION mbi;
    SYSTEM_INFO si;
    DWORD PageSize;
    DWORD flNewProtect;
    DWORD flOldProtect;

    // Use _alloca() to get the current stack pointer

    pStack = _alloca(1);

    // Find the base of the stack.

    if (VirtualQuery(pStack, &mbi, sizeof mbi) == 0)
        return 0;
    pStackBase = mbi.AllocationBase;

    // Find the page just below where the stack pointer currently points.
    // This is the highest potential guard page.

    GetSystemInfo(&si);
    PageSize = si.dwPageSize;

    pMaxGuard = (LPBYTE) (((DWORD_PTR)pStack & ~(DWORD_PTR)(PageSize - 1))
                       - PageSize);

    // If the potential guard page is too close to the start of the stack
    // region, abandon the reset effort for lack of space.  Win9x has a
    // larger reserved stack requirement.

    pMinGuard = pStackBase + ((_osplatform == VER_PLATFORM_WIN32_WINDOWS)
                              ? MIN_STACK_REQ_WIN9X
                              : MIN_STACK_REQ_WINNT) * PageSize;

    if (pMaxGuard < pMinGuard)
        return 0;

    // On a non-Win9x system, do nothing if a guard page is already present,
    // else set up the guard page to the bottom of the committed range,
    // allowing for the reserved stack requirement.
    // For Win9x, just set guard page below the current stack page.

    if (_osplatform != VER_PLATFORM_WIN32_WINDOWS) {

        // Find first block of committed memory in the stack region

        pGuard = pStackBase;
        do {
            if (VirtualQuery(pGuard, &mbi, sizeof mbi) == 0)
                return 0;
            pGuard = pGuard + mbi.RegionSize;
        } while ((mbi.State & MEM_COMMIT) == 0);
        pGuard = mbi.BaseAddress;

        // If first committed block is already marked as a guard page,
        // there is nothing that needs to be done, so return success.

        if (mbi.Protect & PAGE_GUARD)
            return 1;

        // Fail if the first committed block is above the highest potential
        // guard page.  Should never happen.

        if (pMaxGuard < pGuard)
            return 0;

        // Make sure to leave enough room so the next overflow will have
        // the proper reserved stack requirement available.

        if (pGuard < pMinGuard)
            pGuard = pMinGuard;

        VirtualAlloc(pGuard, PageSize, MEM_COMMIT, PAGE_READWRITE);
    }
    else {
        pGuard = pMaxGuard;
    }

    // Enable the new guard page.

    flNewProtect = _osplatform == VER_PLATFORM_WIN32_WINDOWS
                   ? PAGE_NOACCESS
                   : PAGE_READWRITE | PAGE_GUARD;

    return VirtualProtect(pGuard, PageSize, flNewProtect, &flOldProtect);
}
