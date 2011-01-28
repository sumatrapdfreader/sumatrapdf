#include "fitz.h"

#define INT_MAX 2147483647

void *
fz_malloc(int size)
{
	void *p = malloc(size);
	if (!p)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		abort();
	}
	return p;
}

void *
fz_calloc(int count, int size)
{
	void *p;

	if (count > INT_MAX / size || count < 0 || size < 0)
	{
		fprintf(stderr, "fatal error: out of memory (integer overflow)\n");
		abort();
	}

	p = malloc(count * size);
	if (!p)
	{
		fprintf(stderr, "fatal error: out of memory\n");
		abort();
	}
	return p;
}

void *
fz_realloc(void *p, int count, int size)
{
	void *np;

	if (count > INT_MAX / size || count < 0 || size < 0)
	{
		fprintf(stderr, "fatal error: out of memory (integer overflow)\n");
		abort();
	}

	np = realloc(p, count * size);
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
