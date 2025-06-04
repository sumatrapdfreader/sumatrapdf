/*
 * This is a version of the public domain getopt implementation by
 * Henry Spencer originally posted to net.sources.
 *
 * This file is in the public domain.
 */

#include <stdio.h>
#include <string.h>

#include "mupdf/fitz/getopt.h"
#include "mupdf/fitz/string-util.h"

char *fz_optarg; /* Global argument pointer. */
int fz_optind = 0; /* Global argv index. */
const fz_getopt_long_options *fz_optlong = NULL;
int fz_optitem = 0; /* Which item in a long opt list is selected? */

static int
match_long_option(int argc, char * const *argv, const fz_getopt_long_options *longopts, char *opt)
{
	if (longopts == NULL)
	{
		fprintf(stderr, "%s: unknown option --%s\n", argv[0], opt);
		return '?';
	}

	while (longopts->option)
	{
		const char *s = longopts->option;
		const char *eq = strchr(s, '=');
		size_t z = eq ? (size_t)(eq-s) : strlen(s);
		int arg = 0; /* No arg */

		if (s[z] == '=')
			arg = 1; /* arg = 1 => Arg list given */
		else if (z && s[z-1] == ':')
			arg = 2, z--; /* arg = 2 -> argument expected */

		/* If we don't match, try the next one. */
		/* If we aren't expecting an argument, and the given string is longer than expected, try the next one. */
		/* If we are expecting an argument, and the given string doesn't either terminate or end in '=', try the next one. */
		if (strncmp(s, opt, z) || (arg == 0 && opt[z] != 0) || (arg != 0 && opt[z] != 0 && opt[z] != '='))
		{
			longopts++;
			continue;
		}

		/* So we have a match. */
		fz_optind++;
		if (opt[z])
			fz_optarg = &opt[z+1];
		else if (fz_optind < argc)
			fz_optarg = argv[fz_optind++];
		else
		{
			fprintf(stderr, "%s: option requires argument --%s\n", argv[0], s);
			return ':';
		}

		/* If there is an arg from a predefined list, try to match that here. */
		if (arg == 1)
		{
			fz_optitem = fz_opt_from_list(&opt[z+1], &s[z+1]);
			if (longopts->flag)
				*longopts->flag = fz_optitem;
			if (fz_optitem < 0)
				return '?';
		}
		else if (arg == 2 && longopts->flag)
		{
			char *p = strchr(fz_optarg, ',');
			if (*fz_optarg == 0)
			{
				fprintf(stderr, "%s: option requires argument --%s\n", argv[0], s);
				return ':';
			}
			else if (!fz_strcasecmp(fz_optarg, "yes") || !fz_strcasecmp(fz_optarg, "on") || !fz_strcasecmp(fz_optarg, "true"))
				*longopts->flag = 1;
			else if (!fz_strcasecmp(fz_optarg, "no") || !fz_strcasecmp(fz_optarg, "off") || !fz_strcasecmp(fz_optarg, "false"))
				*longopts->flag = 0;
			else
				*longopts->flag = fz_atoi(fz_optarg);
			fz_optarg = p ? p+1 : NULL;
		}
		fz_optlong = longopts;
		return 0;
	}

	fprintf(stderr, "%s: unknown option --%s\n", argv[0], opt);
	return '?';
}

static char *scan = NULL; /* Private scan pointer. */

int
fz_getopt_long(int argc, char * const *argv, const char *optstring, const fz_getopt_long_options *longopts)
{
	int c;
	const char *place;

	fz_optarg = NULL;
	fz_optlong = NULL;
	fz_optitem = -1;

	while (!scan || *scan == '\0')
	{
		if (fz_optind == 0)
			fz_optind++;

		if (fz_optind >= argc || argv[fz_optind][0] != '-' || argv[fz_optind][1] == '\0')
			return EOF;
		if (argv[fz_optind][1] == '-' && argv[fz_optind][2] == '\0') {
			fz_optind++;
			return EOF;
		}
		if (argv[fz_optind][1] == '-')
		{
			scan = NULL;
			return match_long_option(argc, argv, longopts, &argv[fz_optind][2]);
		}

		scan = argv[fz_optind]+1;
		fz_optind++;
	}

	c = *scan++;
	place = strchr(optstring, c);

	if (!place || c == ':') {
		fprintf(stderr, "%s: unknown option -%c\n", argv[0], c);
		return '?';
	}

	place++;
	if (*place == ':') {
		if (*scan != '\0') {
			fz_optarg = scan;
			scan = NULL;
		} else if( fz_optind < argc ) {
			fz_optarg = argv[fz_optind];
			fz_optind++;
		} else {
			fprintf(stderr, "%s: option requires argument -%c\n", argv[0], c);
			return ':';
		}
	}

	return c;
}

int
fz_getopt(int argc, char *const *argv, const char *optstring)
{
	return fz_getopt_long(argc, argv, optstring, NULL);
}

int
fz_opt_from_list(char *opt, const char *optlist)
{
	int n = 0;

	while (*optlist)
	{
		const char *optend = optlist;

		if (*optend == '*')
		{
			fz_optarg = opt;
			return n;
		}

		while (*optend != 0 && *optend != '|' && *optend != ':')
			optend++;

		if (fz_strncasecmp(optlist, opt, optend-optlist))
		{
			/* We didn't match. Carry on. */
		}
		else if (opt[optend-optlist] == 0)
		{
			/* We matched, ending in NUL */
			fz_optarg = NULL;
			return n;
		}
		else if (*optend == ':' && opt[optend-optlist] == ':')
		{
			/* We matched, and we both have some arguments and expected some. */
			fz_optarg = &opt[optend-optlist+1];
			return n;
		}

		n++;
		if (*optend == ':')
		{
			optend++;
			if (*optend == '|')
				optend++;
			else if (*optend != 0)
			{
				fprintf(stderr, "Malformed options string");
				return -1;
			}
		}
		if (*optend == '|')
			optend++;
		optlist = optend;
	}

	fprintf(stderr, "Unrecognised option argument: %s\n", opt);
	return -1;
}

char *
fz_optpath(char *opt)
{
	if (!strcmp(opt, "-"))
		return "/dev/stdout";
#ifdef _WIN32
	if (!fz_strcasecmp(opt, "con"))
		return "/dev/stdout";
	if (!fz_strcasecmp(opt, "nul"))
		return "/dev/null";
#endif
	return opt;
}
