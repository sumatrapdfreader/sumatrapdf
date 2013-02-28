/*
 * mutool -- swiss army knife of pdf manipulation tools
 */

#include <fitz.h>

int pdfclean_main(int argc, char *argv[]);
int pdfextract_main(int argc, char *argv[]);
int pdfinfo_main(int argc, char *argv[]);
int pdfposter_main(int argc, char *argv[]);
int pdfshow_main(int argc, char *argv[]);

static struct {
	int (*func)(int argc, char *argv[]);
	char *name;
	char *desc;
} tools[] = {
	{ pdfclean_main, "clean", "rewrite pdf file" },
	{ pdfextract_main, "extract", "extract font and image resources" },
	{ pdfinfo_main, "info", "show information about pdf resources" },
	{ pdfposter_main, "poster", "split large page into many tiles" },
	{ pdfshow_main, "show", "show internal pdf objects" },
};

static int
namematch(const char *end, const char *start, const char *match)
{
	int len = strlen(match);
	return ((end-len >= start) && (strncmp(end-len, match, len) == 0));
}

#ifdef _WIN32_UTF8
static int main_utf8(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	char *start, *end;
	char buf[32];
	int i;

	if (argc == 0)
	{
		fprintf(stderr, "No command name found!\n");
		return 1;
	}

	/* Check argv[0] */

	if (argc > 0)
	{
		end = start = argv[0];
		while (*end)
			end++;
		if ((end-4 >= start) && (end[-4] == '.') && (end[-3] == 'e') && (end[-2] == 'x') && (end[-1] == 'e'))
			end = end-4;
		for (i = 0; i < nelem(tools); i++)
		{
			strcpy(buf, "mupdf");
			strcat(buf, tools[i].name);
			if (namematch(end, start, buf) || namematch(end, start, buf+2))
				return tools[i].func(argc, argv);
		}
	}

	/* Check argv[1] */

	if (argc > 1)
	{
		for (i = 0; i < nelem(tools); i++)
			if (!strcmp(tools[i].name, argv[1]))
				return tools[i].func(argc - 1, argv + 1);
	}

	/* Print usage */

	fprintf(stderr, "usage: mutool <command> [options]\n");

	for (i = 0; i < nelem(tools); i++)
		fprintf(stderr, "\t%s\t-- %s\n", tools[i].name, tools[i].desc);

	return 1;
}

#ifdef _WIN32_UTF8
static char *
wchar_to_utf8(wchar_t *s)
{
	wchar_t *src = s;
	char *d;
	char *dst;
	int len = 1;

	while (*src)
	{
		len += fz_runelen(*src++);
	}

	d = malloc(len);
	if (d != NULL)
	{
		dst = d;
		src = s;
		while (*src)
		{
			dst += fz_runetochar(dst, *src++);
		}
		*dst = 0;
	}
	return d;
}

int wmain(int argc, wchar_t *wargv[])
{
	int i, ret;
	char **argv = calloc(argc, sizeof(char *));
	if (argv == NULL)
		goto oom;

	for (i = 0; i < argc; i++)
	{
		argv[i] = wchar_to_utf8(wargv[i]);
		if (argv[i] == NULL)
			goto oom;
	}

	ret = main_utf8(argc, argv);

	if (0)
	{
oom:
		ret = 1;
		fprintf(stderr, "Out of memory while processing command line args\n");
	}
	for (i = 0; i < argc; i++)
	{
		free(argv[i]);
	}
	free(argv);

	return ret;
}
#endif
