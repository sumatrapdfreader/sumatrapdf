/* cmapdump.c -- parse a CMap file and dump it as a c-struct */

#include <stdio.h>
#include <string.h>

#include "fitz.h"
#include "mupdf.h"

#include "../fitz/base_error.c"
#include "../fitz/base_memory.c"
#include "../fitz/base_string.c"
#include "../fitz/stm_buffer.c"
#include "../fitz/stm_open.c"
#include "../fitz/stm_read.c"

#include "../pdf/pdf_lex.c"
#include "../pdf/pdf_cmap.c"
#include "../pdf/pdf_cmap_parse.c"

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
		fprintf(stderr, "cmapdump: could not open output file '%s'\n", argv[1]);
		return 1;
	}

	fprintf(fo, "/* This is an automatically generated file. Do not edit. */\n");

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

		fi = fz_open_file(argv[i]);
		if (!fi)
			fz_throw("cmapdump: could not open input file '%s'\n", argv[i]);

		error = pdf_parse_cmap(&cmap, fi);
		if (error)
		{
			fz_catch(error, "cmapdump: could not parse input cmap '%s'\n", argv[i]);
			return 1;
		}

		fprintf(fo, "\n/* %s */\n\n", cmap->cmap_name);

		fprintf(fo, "static const pdf_range cmap_%s_ranges[] = {", name);
		if (cmap->rlen == 0)
		{
			fprintf(fo, " {0,%d,0}", PDF_CMAP_RANGE);
		}
		for (k = 0; k < cmap->rlen; k++)
		{
			if (k % 4 == 0)
				fprintf(fo, "\n");
			fprintf(fo, "{%d,%d,%d},",
				cmap->ranges[k].low, cmap->ranges[k].extent_flags, cmap->ranges[k].offset);
		}
		fprintf(fo, "\n};\n\n");

		if (cmap->tlen == 0)
		{
			fprintf(fo, "static const unsigned short cmap_%s_table[] = { 0 };\n\n", name);
		}
		else
		{
			fprintf(fo, "static const unsigned short cmap_%s_table[%d] = {",
				name, cmap->tlen);
			for (k = 0; k < cmap->tlen; k++)
			{
				if (k % 12 == 0)
					fprintf(fo, "\n");
				fprintf(fo, "%d,", cmap->table[k]);
			}
			fprintf(fo, "\n};\n\n");
		}

		fprintf(fo, "static pdf_cmap cmap_%s = {\n", name);
		fprintf(fo, "\t-1, ");
		fprintf(fo, "\"%s\", ", cmap->cmap_name);
		fprintf(fo, "\"%s\", 0, ", cmap->usecmap_name);
		fprintf(fo, "%d, ", cmap->wmode);
		fprintf(fo, "%d,\n\t{ ", cmap->codespace_len);
		if (cmap->codespace_len == 0)
		{
			fprintf(fo, "{0,0,0},");
		}
		for (k = 0; k < cmap->codespace_len; k++)
		{
			fprintf(fo, "{%d,%d,%d},",
				cmap->codespace[k].n, cmap->codespace[k].low, cmap->codespace[k].high);
		}
		fprintf(fo, " },\n");

		fprintf(fo, "\t%d, %d, (pdf_range*) cmap_%s_ranges,\n",
			cmap->rlen, cmap->rlen, name);

		fprintf(fo, "\t%d, %d, (unsigned short*) cmap_%s_table,\n",
			cmap->tlen, cmap->tlen, name);

		fprintf(fo, "};\n");

		printf("\t{\"%s\",&cmap_%s},\n", cmap->cmap_name, name);

		fz_close(fi);
	}

	if (fclose(fo))
	{
		fprintf(stderr, "cmapdump: could not close output file '%s'\n", argv[1]);
		return 1;
	}

	return 0;
}
