/* bin2hex.c -- Turn the contents of a file into an array of unsigned chars */

#include <stdio.h>
#include <string.h>

/* We never want to build memento versions of the cquote util */
#undef MEMENTO

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
	FILE *fi, *fo;
	char name[256];
	char *realname;
	int i, j, c;

	if (argc < 3)
	{
		fprintf(stderr, "usage: bin2hex output.h lots of text files\n");
		return 1;
	}

	fo = fopen(argv[1], "wb");
	if (!fo)
	{
		fprintf(stderr, "cquote: could not open output file '%s'\n", argv[1]);
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
			fprintf(stderr, "bin2hex: file name too long\n");
			if (fclose(fo))
			{
				fprintf(stderr, "bin2hex: could not close output file '%s'\n", argv[1]);
				return 1;
			}
			return 1;
		}

		strcpy(name, realname);
		clean(name);

		fi = fopen(argv[i], "rb");

		j = 0;
		while ((c = fgetc(fi)) != EOF)
		{
			if (j != 0)
			{
				fputc(',', fo);
				fputc(j%8 == 0 ? '\n' : ' ', fo);
			}

			fprintf(fo, "0x%02x", c);
			j++;
		}

		fputc('\n', fo);

		if (fclose(fi))
		{
			fprintf(stderr, "bin2hex: could not close input file '%s'\n", argv[i]);
			return 1;
		}

	}

	if (fclose(fo))
	{
		fprintf(stderr, "bin2hex: could not close output file '%s'\n", argv[1]);
		return 1;
	}

	return 0;
}
