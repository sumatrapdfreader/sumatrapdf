// Copyright (C) 2025 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_OPTIONS_H
#define MUPDF_FITZ_OPTIONS_H

#include "mupdf/fitz/context.h"
#include "mupdf/fitz/pool.h"

/**
	An fz_options structure encapsulates a list of key, or
	key=value options, together with details such as whether
	they have been used or not.
*/
typedef struct fz_options fz_options;

/**
	Create an options object, with the initial contents parsed from the string.
	See fz_parse_options for details on the string parsing.
	If the string is NULL, the options object will be initialized but empty.
*/
fz_options *fz_new_options(fz_context *ctx, const char *option_string);

/**
	Parse more options from an options string, and add them to
	an existing fz_options object.

	The parser supports three distinct syntaxes (identified by the leading character).

	- Classic comma separated list of values
		rotate=90,bbox="0,0,100,100",title="Hello, world!"

	- URL query string
		?rotate=90&bbox=0,0,100,100&title=Hello, world!

	- A single JSON object -- no nested objects (except arrays of numbers)
		{"rotate":90,"bbox":[0,0,100,100],"title":"Hello, world!"}
*/
void fz_parse_options(fz_context *ctx, fz_options *options, const char *option_string);

/**
	Take a new reference to the options struct.
*/
fz_options *fz_keep_options(fz_context *ctx, fz_options *opts);

/**
	Drop an fz_options object.
*/
void fz_drop_options(fz_context *ctx, fz_options *opts);

/**
	Check to see if a key is present in the options object.

	If it is not, then return 0.

	If val is non-NULL, *val will be updated to point to the value.

	The option will be recorded as having been accessed.
*/
int fz_lookup_option(fz_context *ctx, fz_options *options, const char *key, const char **val);

/**
	Check to see if a key is a boolean.

	If it is absent, return 0.

	If it is true (empty, 1, yes, true, or enabled), returns 1,
	clears any invalid flag, and counts as accessed.

	If it is false (0, no, false, or disabled), returns 0,
	clears any invalid flag, and counts as accessed.

	If it is absent, or any other value, it is marked as invalid, returns 0.
*/
int fz_lookup_option_yes(fz_context *ctx, fz_options *options, const char *key);

/**
	Check to see if a key is present, and is true or false.

	If it is absent, return 0.

	If it is true (empty, 1, yes, true, or enabled), *x is assigned 1,
	the invalid flag is cleared, marked as accessed, returns 1.

	If it is false (0, no, false, or disabled), *x is assigned 0,
	the invalid flag is clear, marked as accessed, returns 0.

	If it is any other value, it is marked as invalid, returns -1.
*/
int fz_lookup_option_boolean(fz_context *ctx, fz_options *options, const char *key, int *x);

/**
	Check to see if an option is present and is of the given type.

	If an option is absent, this returns 0.

	If an option is present, but not of the required type, it will be
	flagged internally as being invalid, and we will return -1.

	If an option is present, and of the required type, any previously
	set invalid flag will be cleared.

	This means we can (for example) lookup an option as an enum, and then
	safely look for it being an integer if that fails.
*/
int fz_lookup_option_float(fz_context *ctx, fz_options *options, const char *key, float *x);
int fz_lookup_option_integer(fz_context *ctx, fz_options *options, const char *key, int *x);
int fz_lookup_option_unsigned(fz_context *ctx, fz_options *options, const char *key, unsigned int *x);

typedef struct
{
	const char *key;
	int val;
} fz_option_enums;

/*
	Check to see if an option is present. Look for it in the given fz_option_enums
	array, which is expected to end with an entry with a NULL key.

	If the option is not found, return 0.

	If the option is found, and a matching entry in the list exists, then *x will
	be set to the 'val' from that entry, the invalid flag is cleared, and we will return 1.

	If the option is found, and no matching entry in the list exists, then *x will
	be set to the 'val' from the terminating NULL entry, the invalid flag will be set,
	and we will return -1.
*/
int fz_lookup_option_enum(fz_context *ctx, fz_options *options, const char *key, int *x,
	const fz_option_enums *enum_list);

/**
	This should be called by any consumer of options after it has looked up
	the options it understands. This will throw if any options were found to
	be flagged as being invalid.
*/
void fz_validate_options(fz_context *ctx, fz_options *options, const char *prefix);

/**
	Warn for any options being unused. Throw if any options are invalid.

	Either this, or fz_throw_on_unused_options should always be called
	(in non-error cases at least) before dropping options.

	Returns 0 if OK, non-zero otherwise.
*/
void fz_warn_on_unused_options(fz_context *ctx, fz_options *options, const char *prefix);

/**
	Throw for any options being unused or invalid.

	Either this, or fz_warn_on_unused_options should always be called
	(in non-error cases at least) before dropping options.

	Returns 0 if OK, non-zero otherwise.
*/
void fz_throw_on_unused_options(fz_context *ctx, fz_options *options, const char *prefix);


/**
	Count the number of options in an options structure.
*/
int fz_count_options(fz_context *ctx, fz_options *options);

/**
	Get an option by index.
*/
const char *fz_get_option_by_index(fz_context *ctx, fz_options *options, int i, const char **val);

/**
	Mark a given option index as being accessed.
*/
void fz_access_option_by_index(fz_context *ctx, fz_options *options, int i);

/**
	Implementation details: subject to change. Only public for
	SWIG built wrappers.
*/

typedef struct fz_option
{
	int flags;
	char *key;
	char *val;
	struct fz_option *next;
} fz_option;

struct fz_options {
	int refs;
	fz_pool *pool;
	fz_option *head;
};

#endif
