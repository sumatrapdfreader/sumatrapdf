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
		fprintf(fo, "0x%02x,", c);
		if (n % 16 == 15)
			fprintf(fo, "\n");
		c = fgetc(fi);
		n ++;
	}

	return n;
}

int
main(int argc, char **argv)
{
	FILE *fo;
	FILE *fi;
	char name[256];
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

	for (i = 2; i < argc; i++)
	{
		fi = fopen(argv[i], "rb");
		if (!fi)
		{
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
		strcpy(name, basename);
		p = name;
		while (*p)
		{
			if ((*p == '/') || (*p == '.') || (*p == '\\') || (*p == '-'))
				*p = '_';
			p ++;
		}

		fseek(fi, 0, SEEK_END);
		len = ftell(fi);
		fseek(fi, 0, SEEK_SET);

		fprintf(fo, "const unsigned int pdf_font_%s_len = %d;\n", name, len);

		fprintf(fo, "#ifdef __linux__\n");
		fprintf(fo, "asm(\".globl pdf_font_%s_buf\");\n", name);
		fprintf(fo, "asm(\".balign 8\");\n");
		fprintf(fo, "asm(\"pdf_font_%s_buf:\");\n", name);
		fprintf(fo, "asm(\".incbin \\\"%s\\\"\");\n", argv[i]);
		fprintf(fo, "#else\n");
		fprintf(fo, "const unsigned char pdf_font_%s_buf[%d] = {\n", name, len);
		hexdump(fo, fi);
		fprintf(fo, "};\n");
		fprintf(fo, "#endif\n");

		fclose(fi);
	}

	return 0;
}
