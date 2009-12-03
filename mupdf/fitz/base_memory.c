#include "fitz_base.h"

void * fz_malloc(int n)
{
	void *p = malloc(n);
	if (!p)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		abort();
	}
	return p;
}

void * fz_realloc(void *p, int n)
{
	void *np = realloc(p, n);
	if (np == nil)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		abort();
	}
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
	{
		fprintf(stderr, "fatal error: out of memory\n");
		abort();
	}
	memcpy(ns, s, strlen(s) + 1);
	return ns;
}

