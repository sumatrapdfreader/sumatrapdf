/* hexdump.c -- an "xxd -i" workalike */

#include <stdio.h>
#include <string.h>

int
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
    char *p;
    int i, len;

    if (argc < 3)
    {
	fprintf(stderr, "usage: hexdump output.c input.dat\n");
	return 1;
    }

    fo = fopen(argv[1], "wb");
    if (!fo)
    {
	fprintf(stderr, "hexdump: could not open output file\n");
	return 1;
    }

    for (i = 2; i < argc; i++)
    {
	fi = fopen(argv[i], "rb");
	if (!fi)
	{
	    fprintf(stderr, "hexdump: could not open input file\n");
	    return 1;
	}

	strcpy(name, argv[i]);
	p = name;
	while (*p)
	{
	    if ((*p == '/') || (*p == '.') || (*p == '\\') || (*p == '-'))
		*p = '_';
	    p ++;
	}

	fprintf(fo, "const unsigned char %s[] = {\n", name);

	len = hexdump(fo, fi);

	fprintf(fo, "};\n");
	fprintf(fo, "const unsigned int %s_len = %d;\n", name, len);

	fclose(fi);
    }

    return 0;
}

