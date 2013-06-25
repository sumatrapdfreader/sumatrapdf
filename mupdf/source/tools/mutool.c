/*
 * mutool -- swiss army knife of pdf manipulation tools
 */

#include "mupdf/fitz.h"

#ifdef _MSC_VER
#define main main_utf8
#endif

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

int main(int argc, char **argv)
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

#ifdef _MSC_VER
int wmain(int argc, wchar_t *wargv[])
{
	char **argv = fz_argv_from_wargv(argc, wargv);
	int ret = main(argc, argv);
	fz_free_argv(argc, argv);
	return ret;
}
#endif
