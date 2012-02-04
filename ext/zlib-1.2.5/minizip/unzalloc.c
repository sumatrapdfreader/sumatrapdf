/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "unzalloc.h"

static UnzipAllocFuncs *gAllocFuncs = NULL;

void unzSetAllocFuncs(UnzipAllocFuncs *funcs)
{
    gAllocFuncs = funcs;
}

void *unzAlloc(size_t size)
{
    if (gAllocFuncs)
        return gAllocFuncs->zip_alloc(NULL, 1, size);
    else
        return malloc(size);
}

void unzFree(void *p)
{
    if (gAllocFuncs)
        gAllocFuncs->zip_free(NULL, p);
    else
        free(p);
}
