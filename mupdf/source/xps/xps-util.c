#include "mupdf/xps.h"

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

/* A URL is defined as consisting of a:
 * SCHEME (e.g. http:)
 * AUTHORITY (username, password, hostname, port, eg //test:passwd@mupdf.com:999)
 * PATH (e.g. /download)
 * QUERY (e.g. ?view=page)
 * FRAGMENT (e.g. #fred) (not strictly part of the URL)
 */
static char *
skip_scheme(char *path)
{
	char *p = path;

	/* Skip over: alpha *(alpha | digit | "+" | "-" | ".") looking for : */
	if (*p >= 'a' && *p <= 'z')
	{}
	else if (*p >= 'A' && *p <= 'Z')
	{}
	else
		return path;

	while (*++p)
	{
		if (*p >= 'a' && *p <= 'z')
		{}
		else if (*p >= 'A' && *p <= 'Z')
		{}
		else if (*p >= '0' && *p <= '9')
		{}
		else if (*p == '+')
		{}
		else if (*p == '-')
		{}
		else if (*p == '.')
		{}
		else if (*p == ':')
			return p+1;
		else
			break;
	}
	return path;
}

static char *
skip_authority(char *path)
{
	char *p = path;

	/* Authority section must start with '//' */
	if (p[0] != '/' || p[1] != '/')
		return path;
	p += 2;

	/* Authority is terminated by end of URL, '/' or '?' */
	while (*p && *p != '/' && *p != '?')
		p++;

	return p;
}

#define SEP(x) ((x)=='/' || (x) == 0)

static char *
xps_clean_path(char *name)
{
	char *p, *q, *dotdot, *start;
	int rooted;

	start = skip_scheme(name);
	start = skip_authority(start);
	rooted = start[0] == '/';

	/*
	 * invariants:
	 *		p points at beginning of path element we're considering.
	 *		q points just past the last path element we wrote (no slash).
	 *		dotdot points just past the point where .. cannot backtrack
	 *				any further (no slash).
	 */
	p = q = dotdot = start + rooted;
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
				if (q != start)
					*q++ = '/';
				*q++ = '.';
				*q++ = '.';
				dotdot = q;
			}
		}
		else /* real path element */
		{
			if (q != start+rooted)
				*q++ = '/';
			while ((*q = *p) != '/' && *q != 0)
				p++, q++;
		}
	}

	if (q == start) /* empty string is really "." */
		*q++ = '.';
	*q = '\0';

	return name;
}

void
xps_resolve_url(char *output, char *base_uri, char *path, int output_size)
{
	char *p = skip_authority(skip_scheme(path));

	if (p != path || path[0] == '/')
	{
		fz_strlcpy(output, path, output_size);
	}
	else
	{
		int len = fz_strlcpy(output, base_uri, output_size);
		if (len == 0 || output[len-1] != '/')
			fz_strlcat(output, "/", output_size);
		fz_strlcat(output, path, output_size);
	}
	xps_clean_path(output);
}

int
xps_url_is_remote(char *path)
{
	char *p = skip_authority(skip_scheme(path));

	return p != path;
}
