#include "fitz.h"
#include "muxps.h"

static inline int xps_tolower(int c)
{
	if (c >= 'A' && c <= 'Z')
		return c + 32;
	return c;
}

int
xps_strcasecmp(char *a, char *b)
{
	while (xps_tolower(*a) == xps_tolower(*b))
	{
		if (*a++ == 0)
			return 0;
		b++;
	}
	return xps_tolower(*a) - xps_tolower(*b);
}

#define SEP(x)	((x)=='/' || (x) == 0)

static char *
xps_clean_path(char *name)
{
	char *p, *q, *dotdot;
	int rooted;

	rooted = name[0] == '/';

	/*
	 * invariants:
	 *		p points at beginning of path element we're considering.
	 *		q points just past the last path element we wrote (no slash).
	 *		dotdot points just past the point where .. cannot backtrack
	 *				any further (no slash).
	 */
	p = q = dotdot = name + rooted;
	while (*p)
	{
		if(p[0] == '/') /* null element */
			p++;
		else if (p[0] == '.' && SEP(p[1]))
			p += 1; /* don't count the separator in case it is nul */
		else if (p[0] == '.' && p[1] == '.' && SEP(p[2]))
		{
			p += 2;
			if (q > dotdot) /* can backtrack */
			{
				while(--q > dotdot && *q != '/')
					;
			}
			else if (!rooted) /* /.. is / but ./../ is .. */
			{
				if (q != name)
					*q++ = '/';
				*q++ = '.';
				*q++ = '.';
				dotdot = q;
			}
		}
		else /* real path element */
		{
			if (q != name+rooted)
				*q++ = '/';
			while ((*q = *p) != '/' && *q != 0)
				p++, q++;
		}
	}

	if (q == name) /* empty string is really "." */
		*q++ = '.';
	*q = '\0';

	return name;
}

void
xps_absolute_path(char *output, char *base_uri, char *path, int output_size)
{
	if (path[0] == '/')
	{
		fz_strlcpy(output, path, output_size);
	}
	else
	{
		fz_strlcpy(output, base_uri, output_size);
		fz_strlcat(output, "/", output_size);
		fz_strlcat(output, path, output_size);
	}
	xps_clean_path(output);
}
