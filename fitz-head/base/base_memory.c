#include "fitz-base.h"

/* Make this thread local storage if you wish.  */

static void *stdmalloc(fz_memorycontext *mem, int n)
{
    return malloc(n);
}

static void *stdrealloc(fz_memorycontext *mem, void *p, int n)
{
    return realloc(p, n);
}

static void stdfree(fz_memorycontext *mem, void *p)
{
    free(p);
}

static fz_memorycontext defmem = { stdmalloc, stdrealloc, stdfree };
static fz_memorycontext *curmem = &defmem;

fz_error fz_koutofmem = {
    -1,
    {"out of memory"}, 
    {"<internal>"},
    {"<internal>"},
    0, 0
};

fz_memorycontext *
fz_currentmemorycontext()
{
    return curmem;
}

void
fz_setmemorycontext(fz_memorycontext *mem)
{
    curmem = mem;
}

void *
fz_malloc(int n)
{
    fz_memorycontext *mem = fz_currentmemorycontext();
    void *p = mem->malloc(mem, n);
    if (!p)
        fz_warn("cannot malloc %d bytes", n);
    return p;
}

void *
fz_realloc(void *p, int n)
{
    fz_memorycontext *mem = fz_currentmemorycontext();
    void *np = mem->realloc(mem, p, n);
    if (np == nil)
       fz_warn("cannot realloc %d bytes", n);
    return np;
}

void
fz_free(void *p)
{
    fz_memorycontext *mem = fz_currentmemorycontext();
    mem->free(mem, p);
}

char *
fz_strdup(char *s)
{
    int len = strlen(s);
    char *ns = fz_malloc(len + 1);
    if (ns)
        strcpy(ns, s);
    return ns;
}

