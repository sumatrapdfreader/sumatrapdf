/*
 * pdfbusy -- combined exe build
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int pdfclean_main(int argc, char *argv[]);
int draw_main(int argc, char *argv[]);
int pdfextract_main(int argc, char *argv[]);
int pdfinfo_main(int argc, char *argv[]);
int pdfshow_main(int argc, char *argv[]);

static int
namematch(const char *end, const char *start, const char *match, int len)
{
	return ((end-len >= start) && (strncmp(end-len, match, len) == 0));
}

int main(int argc, char **argv)
{
	char *start, *end;
	if (argc == 0)
	{
		fprintf(stderr, "No command name found!\n");
		exit(EXIT_FAILURE);
	}

	end = start = argv[0];
	while (*end)
		end++;
	if ((end-4 >= start) && (end[-4] == '.') && (end[-3] == 'e') && (end[-2] == 'x') && (end[-1] == 'e'))
		end = end-4;
	if (namematch(end, start, "mupdfdraw", 9) || namematch(end, start, "muxpsdraw", 9) || namematch(end, start, "mudraw", 6))
		return draw_main(argc, argv);
	if (namematch(end, start, "mupdfclean", 10))
		return pdfclean_main(argc, argv);
	if (namematch(end, start, "mupdfextract", 12))
		return pdfextract_main(argc, argv);
	if (namematch(end, start, "mupdfshow", 9))
		return pdfshow_main(argc, argv);
	if (namematch(end, start, "mupdfinfo", 9))
		return pdfinfo_main(argc, argv);
	/* And include old names for backward compatibility */
	if (namematch(end, start, "pdfdraw", 7) || namematch(end, start, "xpsdraw", 7))
		return draw_main(argc, argv);
	if (namematch(end, start, "pdfclean", 8))
		return pdfclean_main(argc, argv);
	if (namematch(end, start, "pdfextract", 10))
		return pdfextract_main(argc, argv);
	if (namematch(end, start, "pdfshow", 7))
		return pdfshow_main(argc, argv);
	if (namematch(end, start, "pdfinfo", 7))
		return pdfinfo_main(argc, argv);

	fprintf(stderr, "mubusy: Combined build of mupdf/mudraw tools.\n\n");
	fprintf(stderr, "Invoke as one of the following:\n");
	fprintf(stderr, "\tmupdfclean, mudraw, mupdfextract, mupdfinfo, mupdfshow.\n");

	return 0;
}
