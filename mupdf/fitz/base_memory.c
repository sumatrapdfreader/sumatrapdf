#include "fitz.h"

void *
fz_malloc(int n)
{
	void *p = malloc(n);
	if (!p)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		abort();
	}
	return p;
}

void *
fz_realloc(void *p, int n)
{
	void *np = realloc(p, n);
	if (np == nil)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		abort();
	}
	return np;
}

void
fz_free(void *p)
{
	free(p);
}

char *
fz_strdup(char *s)
{
	int len = strlen(s) + 1;
	char *ns = fz_malloc(len);
	memcpy(ns, s, len);
	return ns;
}
