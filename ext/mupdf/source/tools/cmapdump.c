/* cmapdump.c -- parse a CMap file and dump it as a c-struct */

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <stdio.h>
#include <string.h>

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
cmapdump_main(int argc, char **argv)
{
	pdf_cmap *cmap;
	fz_stream *fi;
	char name[256];
	int i, k;
	fz_context *ctx;

	if (argc < 2)
	{
		fprintf(stderr, "usage: cmapdump > out.c [lots of cmap files]\n");
		return 1;
	}

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		return 1;
	}

	printf("/* This is an automatically generated file. Do not edit. */\n");

	for (i = 1; i < argc; i++)
	{
		fi = fz_open_file(ctx, argv[i]);
		cmap = pdf_load_cmap(ctx, fi);
		fz_drop_stream(ctx, fi);

		strcpy(name, cmap->cmap_name);
		clean(name);

		printf("\n/* %s */\n\n", cmap->cmap_name);

		if (cmap->rlen)
		{
			printf("static const pdf_range cmap_%s_ranges[] = {", name);
			for (k = 0; k < cmap->rlen; k++)
			{
				printf("\n{%u,%u,%u},", cmap->ranges[k].low, cmap->ranges[k].high, cmap->ranges[k].out);
			}
			printf("\n};\n\n");
		}

		if (cmap->xlen)
		{
			printf("static const pdf_xrange cmap_%s_xranges[] = {", name);
			for (k = 0; k < cmap->xlen; k++)
			{
				printf("\n{%u,%u,%u},", cmap->xranges[k].low, cmap->xranges[k].high, cmap->xranges[k].out);
			}
			printf("\n};\n\n");
		}

		if (cmap->mlen > 0)
		{
			printf("static const pdf_mrange cmap_%s_mranges[] = {", name);
			for (k = 0; k < cmap->mlen; k++)
			{
				printf("\n{%u,%u},", cmap->mranges[k].low, cmap->mranges[k].out);
			}
			printf("\n};\n\n");
		}

		if (cmap->dlen > 0)
		{
			int ti = 0, tn = cmap->dict[0];
			printf("static const int cmap_%s_table[] = {\n", name);
			for (k = 0; k < cmap->dlen; k++)
			{
				if (ti > tn)
				{
					printf("\n");
					ti = 1;
					tn = cmap->dict[k];
				}
				else
					++ti;
				printf("%u,", cmap->dict[k]);
			}
			printf("\n};\n\n");
		}

		printf("static pdf_cmap cmap_%s = {\n", name);
		printf("\t{ -1, pdf_drop_cmap_imp },\n");
		printf("\t/* cmapname */ \"%s\",\n", cmap->cmap_name);
		printf("\t/* usecmap */ \"%s\", NULL,\n", cmap->usecmap_name);
		printf("\t/* wmode */ %u,\n", cmap->wmode);
		printf("\t/* codespaces */ %u, {\n", cmap->codespace_len);
		if (cmap->codespace_len == 0)
		{
			printf("\t\t{ 0, 0, 0 },\n");
		}
		for (k = 0; k < cmap->codespace_len; k++)
		{
			int n = cmap->codespace[k].n;
			printf("\t\t{ %u, 0x%0*x, 0x%0*x },\n", n,
				n*2, cmap->codespace[k].low,
				n*2, cmap->codespace[k].high);
		}
		printf("\t},\n");

		if (cmap->rlen)
			printf("\t%u, %u, (pdf_range*)cmap_%s_ranges,\n", cmap->rlen, cmap->rlen, name);
		else
			printf("\t0, 0, NULL, /* ranges */\n");
		if (cmap->xlen)
			printf("\t%u, %u, (pdf_xrange*)cmap_%s_xranges,\n", cmap->xlen, cmap->xlen, name);
		else
			printf("\t0, 0, NULL, /* xranges */\n");
		if (cmap->mlen)
			printf("\t%u, %u, (pdf_mrange*)cmap_%s_mranges,\n", cmap->mlen, cmap->mlen, name);
		else
			printf("\t0, 0, NULL, /* mranges */\n");
		if (cmap->dict)
			printf("\t%u, %u, (int*)cmap_%s_table,\n", cmap->dlen, cmap->dlen, name);
		else
			printf("\t0, 0, NULL, /* table */\n");
		printf("\t0, 0, 0, NULL /* splay tree */\n");
		printf("};\n");

		pdf_drop_cmap(ctx, cmap);
	}

	fz_drop_context(ctx);
	return 0;
}
