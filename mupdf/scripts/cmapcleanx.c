/* cmapclean.c -- parse a CMap file and write it back out */

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
#include "../source/fitz/printf.c"

#include "../source/pdf/pdf-lex.c"
#include "../source/pdf/pdf-cmap.c"
#include "../source/pdf/pdf-cmap-parse.c"

struct cidrange {
	unsigned int lo, hi, v;
};

static int cmpcidrange(const void *va, const void *vb)
{
	unsigned int a = ((const struct cidrange *)va)->lo;
	unsigned int b = ((const struct cidrange *)vb)->lo;
	return a < b ? -1 : a > b ? 1 : 0;
}

static void pc(unsigned int c)
{
	if (c <= 0xff) printf("<%02x>", c);
	else if (c <= 0xffff) printf("<%04x>", c);
	else if (c <= 0xffffff) printf("<%06x>", c);
	else printf("<%010x>", c);
}

int
main(int argc, char **argv)
{
	fz_context *ctx;
	fz_stream *fi;
	pdf_cmap *cmap;
	int k, m, n, i;
	struct cidrange *r;

	if (argc != 2)
	{
		fprintf(stderr, "usage: cmapclean input.cmap\n");
		return 1;
	}

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		return 1;
	}

	fi = fz_open_file(ctx, argv[1]);
	cmap = pdf_load_cmap(ctx, fi);
	fz_close(fi);

	printf("begincmap\n");
	printf("/CMapName /%s def\n", cmap->cmap_name);
	printf("/WMode %d def\n", cmap->wmode);
	if (cmap->usecmap_name[0])
		printf("/%s usecmap\n", cmap->usecmap_name);

	if (cmap->codespace_len)
	{
		printf("begincodespacerange\n");
		for (k = 0; k < cmap->codespace_len; k++)
		{
			if (cmap->codespace[k].n == 1)
				printf("<%02x> <%02x>\n", cmap->codespace[k].low, cmap->codespace[k].high);
			else if (cmap->codespace[k].n == 2)
				printf("<%04x> <%04x>\n", cmap->codespace[k].low, cmap->codespace[k].high);
			else if (cmap->codespace[k].n == 3)
				printf("<%06x> <%06x>\n", cmap->codespace[k].low, cmap->codespace[k].high);
			else if (cmap->codespace[k].n == 4)
				printf("<%08x> <%08x>\n", cmap->codespace[k].low, cmap->codespace[k].high);
			else
				printf("<%x> <%x>\n", cmap->codespace[k].low, cmap->codespace[k].high);
		}
		printf("endcodespacerange\n");
	}

	n = cmap->rlen + cmap->xlen;
	r = malloc(n * sizeof *r);
	i = 0;

	for (k = 0; k < cmap->rlen; k++) {
		r[i].lo = cmap->ranges[k].low;
		r[i].hi = cmap->ranges[k].high;
		r[i].v = cmap->ranges[k].out;
		++i;
	}

	for (k = 0; k < cmap->xlen; k++) {
		r[i].lo = cmap->xranges[k].low;
		r[i].hi = cmap->xranges[k].high;
		r[i].v = cmap->xranges[k].out;
		++i;
	}

	qsort(r, n, sizeof *r, cmpcidrange);

	if (n)
	{
		printf("begincidchar\n");
		for (i = 0; i < n; ++i)
		{
			for (k = r[i].lo, m = r[i].v; k <= r[i].hi; ++k, ++m)
			{
				pc(k);
				printf("%u\n", m);
			}
		}
		printf("endcidchar\n");
	}

	if (cmap->mlen > 0)
	{
		printf("beginbfchar\n");
		for (k = 0; k < cmap->mlen; k++)
		{
			pc(cmap->mranges[k].low);

			printf("<");
			for (m = 0; m < cmap->mranges[k].len; ++m)
				printf("%04x", cmap->mranges[k].out[m]);
			printf(">\n");
		}
		printf("endbfchar\n");
	}

	printf("endcmap\n");

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

void fz_new_document_handler_context(fz_context *ctx)
{
}

void fz_drop_document_handler_context(fz_context *ctx)
{
}

fz_document_handler_context *fz_keep_document_handler_context(fz_context *ctx)
{
	return NULL;
}
