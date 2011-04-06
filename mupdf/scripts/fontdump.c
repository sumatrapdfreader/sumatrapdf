/* fontdump.c -- an "xxd -i" workalike for dumping binary fonts as source code */

#include <stdio.h>
#include <string.h>

static int
hexdump(FILE *fo, FILE *fi)
{
	int c, n;

	n = 0;
	c = fgetc(fi);
	while (c != -1)
	{
		n += fprintf(fo, "%d,", c);
		if (n > 72) {
			fprintf(fo, "\n");
			n = 0;
		}
		c = fgetc(fi);
	}

	return n;
}

int
main(int argc, char **argv)
{
	FILE *fo;
	FILE *fi;
	char fontname[256];
	char origname[256];
	char *basename;
	char *p;
	int i, len;

	if (argc < 3)
	{
		fprintf(stderr, "usage: fontdump output.c input.dat\n");
		return 1;
	}

	fo = fopen(argv[1], "wb");
	if (!fo)
	{
		fprintf(stderr, "fontdump: could not open output file '%s'\n", argv[1]);
		return 1;
	}

	fprintf(fo, "#ifndef __STRICT_ANSI__\n");
	fprintf(fo, "#if defined(__linux__) || defined(__FreeBSD__)\n");
	fprintf(fo, "#define HAVE_INCBIN\n");
	fprintf(fo, "#endif\n");
	fprintf(fo, "#endif\n");

	for (i = 2; i < argc; i++)
	{
		fi = fopen(argv[i], "rb");
		if (!fi)
		{
			fclose(fo);
			fprintf(stderr, "fontdump: could not open input file '%s'\n", argv[i]);
			return 1;
		}

		basename = strrchr(argv[i], '/');
		if (!basename)
			basename = strrchr(argv[i], '\\');
		if (basename)
			basename++;
		else
			basename = argv[i];

		strcpy(origname, basename);
		p = strrchr(origname, '.');
		if (p) *p = 0;
		strcpy(fontname, origname);

		p = fontname;
		while (*p)
		{
			if (*p == '/' || *p == '.' || *p == '\\' || *p == '-')
				*p = '_';
			p ++;
		}

		fseek(fi, 0, SEEK_END);
		len = ftell(fi);
		fseek(fi, 0, SEEK_SET);

		printf("\t{\"%s\",pdf_font_%s,%d},\n", origname, fontname, len);

		fprintf(fo, "\n#ifdef HAVE_INCBIN\n");
		fprintf(fo, "extern const unsigned char pdf_font_%s[%d];\n", fontname, len);
		fprintf(fo, "asm(\".globl pdf_font_%s\");\n", fontname);
		fprintf(fo, "asm(\".balign 8\");\n");
		fprintf(fo, "asm(\"pdf_font_%s:\");\n", fontname);
		fprintf(fo, "asm(\".incbin \\\"%s\\\"\");\n", argv[i]);
		fprintf(fo, "#else\n");
		fprintf(fo, "static const unsigned char pdf_font_%s[%d] = {\n", fontname, len);
		hexdump(fo, fi);
		fprintf(fo, "};\n");
		fprintf(fo, "#endif\n");

		fclose(fi);
	}

	if (fclose(fo))
	{
		fprintf(stderr, "fontdump: could not close output file '%s'\n", argv[1]);
		return 1;
	}

	return 0;
}
