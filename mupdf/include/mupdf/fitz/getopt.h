// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef MUPDF_FITZ_GETOPT_H
#define MUPDF_FITZ_GETOPT_H

#include "export.h"

typedef struct
{
	char *option;
	int *flag;
	void *opaque;
} fz_getopt_long_options;

/**
	Simple functions/variables for use in tools.

	ostr = option string. Comprises single letter options, followed by : if there
	is an argument to the option.

	longopts: NULL (indicating no long options), or a pointer to an array of
	longoptions, terminated by an entry with option == NULL.

	In the event of matching a single char option, this function will normally
	return the char. The exception to this is when the option requires an
	argument and none is supplied; in this case we return ':'.

	In the event of matching a long option, this function returns 0, with fz_optlong
	set to point to the matching option.

	A long option entry may be followed with : to indicate there is an argument to the
	option. If the need for an argument is specified in this way, and no argument is
	given, an error will be displayed and argument processing will stop. If an argument
	is given, and the long option record contains a non-null flag pointer, then the code
	will decode the argument and fill in that flag pointer. Specifically,
	case-insensitive matches to 'yes', 'no', 'true' and 'false' will cause a value of 0
	or 1 as appropriate to be written; failing this the arg will be interpreted as a
	decimal integer using atoi.

	A long option entry may be followed by an list of options (e.g. myoption=foo|bar|baz)
	and the option will be passed to fz_opt_from_list. The return value of that will be
	placed in fz_optitem. If the return value of that function is -1, then an error will
	be displayed and argument processing will stop.

	In the event of reaching the end of the arg list or '--', this function returns EOF.

	In the event of failing to match anything, an error is printed, and we return '?'.

	If an argument is expected for the option, then fz_optarg will be returned pointing
	at the start of the argument. Examples of supported argument formats: '-r500', '-r 500',
	'--resolution 500', '--resolution=500'.
*/
extern int fz_getopt_long(int nargc, char * const *nargv, const char *ostr, const fz_getopt_long_options *longopts);

/**
	Identical to fz_getopt_long, but with a NULL longopts field, signifying no long
	options.
*/
extern int fz_getopt(int nargc, char * const *nargv, const char *ostr);

/**
	fz_optind is updated to point to the current index being read from the
	arguments.
*/
FZ_DATA extern int fz_optind;

/**
	fz_optarg is a pointer to the argument data for the most recently
	read option.
*/
FZ_DATA extern char *fz_optarg;

/**
	fz_optlong is a pointer to the record for the most recently
	read long option. (i.e. if a long option is detected, this
	will be set to point to the record for that option, otherwise
	it will be NULL).
*/
FZ_DATA extern const fz_getopt_long_options *fz_optlong;

/**
	The item number for the most recently matched item list.

	First item in the list is numbered 0. No match is -1.
*/
FZ_DATA extern int fz_optitem;

/**
	Return the index of a (case-insensitive) option within an optlist.

	For instance for optlist = "Foo|Bar|Baz", and  opt = "bar",
	this would return 1.

	If the optlist ends with "|*" then that is a catch all case and
	matches all options allowing the caller to process it itself.
	fz_optarg will be set to point to the option, and the return
	value will be the index of the '*' option within that list.

	If an optlist entry ends with ':' (e.g. "Foo:") then that option
	may have suboptions appended to it (for example "JPG:80") and
	fz_optarg will be set to point at "80". Otherwise fz_optarg will
	be set to NULL.

	In the event of no-match found, prints an error and returns -1.
*/
int fz_opt_from_list(char *opt, const char *optlist);

/**
	Convert "-" to "/dev/stdout" for use with command lines.
	Also converts "nul" and "con" on Windows.
*/
char *fz_optpath(char *opt);

#endif
