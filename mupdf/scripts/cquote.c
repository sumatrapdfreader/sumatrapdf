/* cquote.c -- Turn the contents of a file into a quoted string */

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
	int i, c;
	int bol = 1;

	if (argc < 3)
	{
		fprintf(stderr, "usage: cquote output.c lots of text files\n");
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
			fprintf(stderr, "cquote: file name too long\n");
			if (fclose(fo))
			{
				fprintf(stderr, "cquote: could not close output file '%s'\n", argv[1]);
				return 1;
			}
			return 1;
		}

		strcpy(name, realname);
		clean(name);

		fi = fopen(argv[i], "rb");

		fprintf(fo, "\n/* %s */\n\n", name);

		c = fgetc(fi);
		while (c != EOF)
		{
			int eol = 0;

			if (bol)
			{
				fputc('\"', fo);
				bol = 0;
			}

			switch (c)
			{
				case '\"':
					fprintf(fo, "\\\"");
					break;

				case '\\':
					fprintf(fo, "\\\\");
					break;

				case '\r':
				case '\n':
					eol = 1;
					break;

				default:
					fputc(c, fo);
					break;
			}

			if (eol)
			{
				fprintf(fo, "\\n\"\n");
				while ((c = fgetc(fi)) == '\r' || c == '\n')
					;
				bol = 1;
			}
			else
			{
				c = fgetc(fi);
			}
		}

		if (!bol)
			fprintf(fo, "\\n\"\n");

		if (fclose(fi))
		{
			fprintf(stderr, "cquote: could not close input file '%s'\n", argv[i]);
			return 1;
		}

	}

	if (fclose(fo))
	{
		fprintf(stderr, "cquote: could not close output file '%s'\n", argv[1]);
		return 1;
	}

	return 0;
}
