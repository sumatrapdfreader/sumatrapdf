#include "fitz_base.h"

void * fz_malloc(int n)
{
    void *p = malloc(n);
    if (!p)
	fz_throw("cannot malloc %d bytes", n);
    return p;
}

void * fz_realloc(void *p, int n)
{
    void *np = realloc(p, n);
    if (np == nil)
	fz_throw("cannot realloc %d bytes", n);
    return np;
}

void fz_free(void *p)
{
    free(p);
}

char * fz_strdup(char *s)
{
    char *ns = malloc(strlen(s) + 1);
    if (!ns)
	fz_throw("cannot strdup %lu bytes", (unsigned long)strlen(s) + 1);
    memcpy(ns, s, strlen(s) + 1);
    return ns;
}

