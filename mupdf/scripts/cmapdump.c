/* cmapdump.c -- parse a CMap file and dump it as a c-struct */

#include <stdio.h>
#include <string.h>

/* We never want to build memento versions of the cmapdump util */
#undef MEMENTO

#include "mupdf/pdf.h"

#include "../source/fitz/context.c"
#include "../source/fitz/error.c"
#include "../source/fitz/memory.c"
#include "../source/fitz/string.c"
#include "../source/fitz/buffer.c"
#include "../source/fitz/stream-open.c"
#include "../source/fitz/stream-read.c"

#include "../source/pdf/pdf-lex.c"
#include "../source/pdf/pdf-cmap.c"
#include "../source/pdf/pdf-cmap-parse.c"

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
	fz_stream *fi;
	FILE *fo;
	char name[256];
	char *realname;
	int i, k;
	fz_context *ctx;

	if (argc < 3)
	{
		fprintf(stderr, "usage: cmapdump output.c lots of cmap files\n");
		return 1;
	}

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		return 1;
	}

#undef fopen
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
			if (fclose(fo))
			{
				fprintf(stderr, "cmapdump: could not close output file '%s'\n", argv[1]);
				return 1;
			}
			return 1;
		}

		strcpy(name, realname);
		clean(name);

		fi = fz_open_file(ctx, argv[i]);
		cmap = pdf_load_cmap(ctx, fi);
		fz_close(fi);

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
		fprintf(fo, "\t{-1, pdf_free_cmap_imp}, ");
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

		if (getenv("verbose"))
			printf("\t{\"%s\",&cmap_%s},\n", cmap->cmap_name, name);
	}

	if (fclose(fo))
	{
		fprintf(stderr, "cmapdump: could not close output file '%s'\n", argv[1]);
		return 1;
	}

	fz_free_context(ctx);
	return 0;
}

void fz_new_font_context(fz_context *ctx)
{
}

void fz_drop_font_context(fz_context *ctx)
{
}

fz_font_context *fz_keep_font_context(fz_context *ctx)
{
	return NULL;
}

void fz_new_colorspace_context(fz_context *ctx)
{
}

void fz_drop_colorspace_context(fz_context *ctx)
{
}

fz_colorspace_context *fz_keep_colorspace_context(fz_context *ctx)
{
	return NULL;
}

void fz_new_aa_context(fz_context *ctx)
{
}

void fz_free_aa_context(fz_context *ctx)
{
}

void fz_copy_aa_context(fz_context *dst, fz_context *src)
{
}

void *fz_keep_storable(fz_context *ctx, fz_storable *s)
{
	return s;
}

void fz_drop_storable(fz_context *ctx, fz_storable *s)
{
}

void fz_new_store_context(fz_context *ctx, unsigned int max)
{
}

void fz_drop_store_context(fz_context *ctx)
{
}

fz_store *fz_keep_store_context(fz_context *ctx)
{
	return NULL;
}

int fz_store_scavenge(fz_context *ctx, unsigned int size, int *phase)
{
	return 0;
}

void fz_new_glyph_cache_context(fz_context *ctx)
{
}

void fz_drop_glyph_cache_context(fz_context *ctx)
{
}

fz_glyph_cache *fz_keep_glyph_cache(fz_context *ctx)
{
	return NULL;
}
