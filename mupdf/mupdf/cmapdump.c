/* cmapdump.c -- parse a CMap file and dump it as a c-struct */

#include <stdio.h>
#include <string.h>

#include "fitz.h"
#include "mupdf.h"

#include "../mupdf/pdf_lex.c"
#include "../mupdf/pdf_cmap.c"
#include "../mupdf/pdf_cmap_parse.c"

static char *
flagtoname(int flag)
{
	switch (flag)
	{
	case PDF_CMAP_SINGLE: return "PDF_CMAP_SINGLE,";
	case PDF_CMAP_RANGE: return "PDF_CMAP_RANGE, ";
	case PDF_CMAP_TABLE: return "PDF_CMAP_TABLE, ";
	case PDF_CMAP_MULTI: return "PDF_CMAP_MULTI, ";
	}
	return "-1,";
}

static void
clean(char *p)
{
	while (*p)
	{
		if ((*p == '/') || (*p == '.') || (*p == '\\') || (*p == '-'))
			*p = '_';
		p ++;
	}
}

int
main(int argc, char **argv)
{
	pdf_cmap *cmap;
	fz_error error;
	fz_stream *fi;
	FILE *fo;
	char name[256];
	char *realname;
	int i, k;

	if (argc < 3)
	{
		fprintf(stderr, "usage: cmapdump output.c lots of cmap files\n");
		return 1;
	}

	fo = fopen(argv[1], "wb");
	if (!fo)
	{
		fprintf(stderr, "cmapdump: could not open output file\n");
		return 1;
	}

	fprintf(fo, "#include \"fitz.h\"\n");
	fprintf(fo, "#include \"mupdf.h\"\n");
	fprintf(fo, "\n");

	for (i = 2; i < argc; i++)
	{
		realname = strrchr(argv[i], '/');
		if (!realname)
			realname = strrchr(argv[i], '\\');
		if (realname)
			realname ++;
		else
			realname = argv[i];

		if (strlen(realname) > (sizeof name - 1))
		{
			fprintf(stderr, "cmapdump: file name too long\n");
			return 1;
		}

		strcpy(name, realname);
		clean(name);

		error = fz_openrfile(&fi, argv[i]);
		if (error)
		{
			fz_catch(error, "cmapdump: could not open input file %s\n", argv[i]);
			return 1;
		}

		error = pdf_parsecmap(&cmap, fi);
		if (error)
		{
			fz_catch(error, "cmapdump: could not parse input cmap %s\n", argv[i]);
			return 1;
		}

		fprintf(fo, "/*\n * %s\n */\n\n", cmap->cmapname);

		fprintf(fo, "static const pdf_range pdf_cmap_%s_ranges[] =\n{\n", name);
		if (cmap->rlen == 0)
		{
			fprintf(fo, "    /* dummy entry for non-c99 compilers */\n");
			fprintf(fo, "    { 0x0, 0x0, PDF_CMAP_RANGE, 0 }\n");
		}
		for (k = 0; k < cmap->rlen; k++)
		{
			fprintf(fo, "    { 0x%04x, 0x%04x, %s %d },\n",
				cmap->ranges[k].low, cmap->ranges[k].high,
				flagtoname(cmap->ranges[k].flag),
				cmap->ranges[k].offset);
		}
		fprintf(fo, "};\n\n");

		if (cmap->tlen == 0)
		{
			fprintf(fo, "static const unsigned short pdf_cmap_%s_table[1] = { 0 };\n\n", name);
		}
		else
		{
			fprintf(fo, "static const unsigned short pdf_cmap_%s_table[%d] =\n{",
				name, cmap->tlen);
			for (k = 0; k < cmap->tlen; k++)
			{
				if (k % 8 == 0)
					fprintf(fo, "\n    ");
				fprintf(fo, "%d, ", cmap->table[k]);
			}
			fprintf(fo, "\n};\n\n");
		}

		fprintf(fo, "pdf_cmap pdf_cmap_%s =\n", name);
		fprintf(fo, "{\n");
		fprintf(fo, "    -1, ");
		fprintf(fo, "\"%s\", ", cmap->cmapname);
		fprintf(fo, "\"%s\", nil, ", cmap->usecmapname);
		fprintf(fo, "%d,\n", cmap->wmode);

		fprintf(fo, "    %d, /* codespace table */\n", cmap->ncspace);
		fprintf(fo, "    {\n");

		if (cmap->ncspace == 0)
		{
			fprintf(fo, "    /* dummy entry for non-c99 compilers */\n");
			fprintf(fo, "    { 0, 0x0, 0x0 },\n");
		}
		for (k = 0; k < cmap->ncspace; k++)
		{
			fprintf(fo, "\t{ %d, 0x%04x, 0x%04x },\n",
				cmap->cspace[k].n, cmap->cspace[k].low, cmap->cspace[k].high);
		}
		fprintf(fo, "    },\n");

		fprintf(fo, "    %d, %d, (pdf_range*) pdf_cmap_%s_ranges,\n",
			cmap->rlen, cmap->rlen, name);

		fprintf(fo, "    %d, %d, (unsigned short*) pdf_cmap_%s_table,\n",
			cmap->tlen, cmap->tlen, name);

		fprintf(fo, "};\n\n");

		fz_dropstream(fi);
	}

	return 0;
}

