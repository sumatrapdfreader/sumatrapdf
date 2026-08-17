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

#include "mupdf/fitz.h"

enum
{
	ACCESSED = 1,
	INVALID = 2,
	NOTIFIED = 4
};

fz_options *
fz_new_options(fz_context *ctx, const char *string)
{
	fz_options *opts = NULL;
	fz_pool *pool = fz_new_pool(ctx);
	fz_try(ctx)
	{
		opts = fz_pool_alloc_struct(ctx, pool, fz_options);
		opts->pool = pool;
		opts->refs = 1;
		if (string)
			fz_parse_options(ctx, opts, string);
	}
	fz_catch(ctx)
	{
		fz_drop_pool(ctx, pool);
		fz_rethrow(ctx);
	}
	return opts;
}

fz_options *
fz_keep_options(fz_context *ctx, fz_options *opts)
{
	return fz_keep_imp(ctx, opts, &opts->refs);
}

void
fz_drop_options(fz_context *ctx, fz_options *opts)
{
	if (fz_drop_imp(ctx, opts, &opts->refs))
	{
		fz_option *opt;
		int bad = 0;

		for (opt = opts->head; opt; opt = opt->next)
			if (!(opt->flags & NOTIFIED) && (!(opt->flags & ACCESSED) || (opt->flags & INVALID)))
				bad = 1;
		if (bad)
			fz_warn(ctx, "dropping unprocessed options");
		fz_drop_pool(ctx, opts->pool);
	}
}

static void
add_option(fz_context *ctx, fz_options *optlist,
	const char *key_start, const char *key_end,
	const char *val_start, const char *val_end,
	void (*unescape)(char *)
) {
	fz_option *opt = fz_pool_alloc_struct(ctx, optlist->pool, fz_option);
	opt->flags = 0;
	opt->key = fz_pool_strndup(ctx, optlist->pool, key_start, key_end - key_start);
	opt->val = fz_pool_strndup(ctx, optlist->pool, val_start, val_end - val_start);
	if (unescape)
	{
		unescape(opt->key);
		unescape(opt->val);
	}
	opt->next = optlist->head;
	optlist->head = opt;
}

static void unescape_csv(char *s)
{
	char *p = s;
	while (*s)
	{
		if (s[0] == '"' && s[1] == '"')
		{
			*p++ = '"';
			s += 2;
		}
		else
		{
			*p++ = *s++;
		}
	}
	*p = 0;
}

static int ishex(int a)
{
	return (a >= 'A' && a <= 'F') ||
		(a >= 'a' && a <= 'f') ||
		(a >= '0' && a <= '9');
}

static int unhex(int c)
{
	if (c >= 'A' && c <= 'F') return c - 'A' + 0xA;
	if (c >= 'a' && c <= 'f') return c - 'a' + 0xA;
	if (c >= '0' && c <= '9') return c - '0';
	return 0;
}

static void unescape_url(char *s)
{
	char *p = s;
	while (*s)
	{
		if (s[0] == '%' && ishex(s[1]) && ishex(s[2]))
		{
			*p++ = (unhex(s[1]) << 4) | unhex(s[2]);
			s += 3;
		}
		else
		{
			*p++ = *s++;
		}
	}
	*p = 0;
}

static void unescape_json(char *s)
{
	char *p = s;
	while (*s)
	{
		if (s[0] == '\\' && s[1])
		{
			*p++ = s[1]; // TODO: unescaping non-identity (\n, \t, etc)?
			s += 2;
		}
		else
		{
			*p++ = *s++;
		}
	}
	*p = 0;
}

static void fz_parse_options_csv(fz_context *ctx, fz_options *options, const char *s)
{
	/* simple list of comma separated key=value pairs */
	/* values may be enclosed in double-quotes; use two double-quotes ("") to represent a double-quote within */
	// example: rotate=90,bbox="0,0,200,100",title="Hello world!"
	const char *k, *kk, *v, *vv;
	while (*s)
	{
		while(*s == ',')
			s++;
		k = s;
		while (*s && *s != ',' && *s != '=')
			++s;
		kk = s;
		if (*s == '=')
		{
			++s;
			if (*s == '"')
			{
				++s;
				v = s;
				while (*s)
				{
					// un-escape double quote ("")
					if (*s == '"')
					{
						if (s[1] == '"')
							++s;
						else
							break;
					}
					++s;
				}
				vv = s;
				if (*s == '"')
					++s;
			}
			else
			{
				v = s;
				while (*s && *s != ',')
					++s;
				vv = s;
			}
		}
		else
		{
			v = vv = s;
		}
		add_option(ctx, options, k, kk, v, vv, unescape_csv);
	}
}

static void fz_parse_options_querystring(fz_context *ctx, fz_options *options, const char *s)
{
	/* simple list of & separated key=value pairs; use %XX url-escape for special characters in keys and values */
	// example: ?rotate=90&bbox=0,0,200,100&title=Hello world!
	const char *k, *kk, *v, *vv;
	while (*s)
	{
		k = s;
		while (*s && *s != '&' && *s != '=')
			++s;
		kk = s;
		if (*s == '=')
		{
			++s;
			v = s;
			while (*s && *s != '&')
				++s;
			vv = s;
		}
		else
		{
			v = vv = s;
		}
		add_option(ctx, options, k, kk, v, vv, unescape_url);
	}
}

static int iswhite(int c)
{
	return c == ' ' || (c >= 8 && c <= 13);
}

static int opt_isdigit(int c)
{
	return (c >= '0' && c <= '9');
}

static int isnumber(int c)
{
	return (c >= '0' && c <= '9') || c == '.' || c == '-';
}

static void fz_parse_options_json(fz_context *ctx, fz_options *options, const char *s)
{
	/* subset of JSON supporting only one top-level object and primitive (boolean,number,string) values */
	/* values may also be a simple (flat) array of numbers (general arrays are not supported) */
	// example: {"rotate":90,"bbox":[0,0,200,100],"title":"Hello, world!"}
	const char *k, *kk, *v, *vv;

	while (iswhite(*s))
		++s;

	while (*s && *s != '}')
	{
		while (iswhite(*s))
			++s;

		// key
		if (*s == '"')
		{
			++s;
			k = s;
			while (*s && *s != '"')
			{
				if (s[0] == '\\' && s[1])
					++s;
				++s;
			}
			kk = s;
			if (*s == '"')
				++s;
		}
		else
		{
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid option syntax (expected string)");
		}

		while (iswhite(*s))
			++s;

		// colon
		if (*s == ':')
			++s;
		else
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid option syntax (expected ':')");

		while (iswhite(*s))
			++s;

		// value
		if (*s == '"')
		{
			// string
			++s;
			v = s;
			while (*s && *s != '"')
			{
				if (s[0] == '\\' && s[1])
					++s;
				++s;
			}
			vv = s;
			if (*s == '"')
				++s;
		}
		else if (*s == '[')
		{
			// array (treat as string of comma separated numbers)
			++s;
			v = s;
			while (*s && *s != ']') {
				if (!iswhite(*s) && !isnumber(*s) && *s != ',')
					fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid option syntax (non-number in array)");
				++s;
			}
			vv = s;
			if (*s == ']')
				++s;
		}
		else if (*s == '-' || (*s >= '0' && *s <= '9'))
		{
			// number
			v = s;
			if (*s == '-')
				++s;
			while (opt_isdigit(*s))
				++s;
			if (*s == '.')
			{
				++s;
				while (opt_isdigit(*s))
					++s;
			}
			if (*s == 'e' || *s == 'E')
			{
				++s;
				if (*s == '+' || *s == '-')
					++s;
				while (opt_isdigit(*s))
					++s;
			}
			vv = s;
		}
		else if (s[0] == 't' && s[1] == 'r' && s[2] == 'u' && s[3] == 'e')
		{
			// true
			v = s;
			s += 4;
			vv = s;
		}
		else if (s[0] == 'f' && s[1] == 'a' && s[2] == 'l' && s[3] == 's' && s[4] == 'e')
		{
			// false
			v = s;
			s += 5;
			vv = s;
		}
		else
		{
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid option syntax (expected boolean, number, or string)");
		}


		while (iswhite(*s))
			++s;

		if (*s == 0)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid option syntax (unterminated token)");

		// optional comma
		if (*s != ',' && *s != '}')
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid option syntax (expected ',' or '}')");

		add_option(ctx, options, k, kk, v, vv, unescape_json);
	}
}

void
fz_parse_options(fz_context *ctx, fz_options *options, const char *s)
{
	if (s[0] == 0)
		return;
	if (s[0] == '{')
		fz_parse_options_json(ctx, options, s+1);
	else if (s[0] == '?')
		fz_parse_options_querystring(ctx, options, s+1);
	else
		fz_parse_options_csv(ctx, options, s);
}

static fz_option *
lookup_option(fz_context *ctx, fz_options *options, const char *key)
{
	fz_option *opt;
	if (options)
	{
		for (opt = options->head; opt; opt = opt->next)
			if (!strcmp(opt->key, key))
				return opt;
	}
	return NULL;
}

static int is_yes_option(const char *val)
{
	return (
		!strcmp(val, "") ||
		!strcmp(val, "1") ||
		!fz_strcasecmp(val, "true") ||
		!fz_strcasecmp(val, "yes") ||
		!fz_strcasecmp(val, "on") ||
		!fz_strcasecmp(val, "enable") ||
		!fz_strcasecmp(val, "enabled")
	);
}

static int is_no_option(const char *val)
{
	return (
		!strcmp(val, "0") ||
		!fz_strcasecmp(val, "false") ||
		!fz_strcasecmp(val, "no") ||
		!fz_strcasecmp(val, "off") ||
		!fz_strcasecmp(val, "disable") ||
		!fz_strcasecmp(val, "disabled")
	);
}

int
fz_lookup_option(fz_context *ctx, fz_options *options, const char *key, const char **val)
{
	fz_option *opt = lookup_option(ctx, options, key);
	if (opt)
	{
		opt->flags |= ACCESSED;
		opt->flags &= ~INVALID;
		return *val = opt->val, 1;
	}
	return 0;
}

int
fz_lookup_option_yes(fz_context *ctx, fz_options *options, const char *key)
{
	fz_option *opt = lookup_option(ctx, options, key);
	if (!opt)
		return 0;

	opt->flags |= ACCESSED;
	if (is_yes_option(opt->val))
	{
		opt->flags &= ~INVALID;
		return 1;
	}
	if (is_no_option(opt->val))
	{
		opt->flags &= ~INVALID;
		return 0;
	}
	opt->flags |= INVALID; // not a boolean
	return -1;
}

int
fz_lookup_option_boolean(fz_context *ctx, fz_options *options, const char *key, int *x)
{
	fz_option *opt = lookup_option(ctx, options, key);
	if (!opt)
		return 0;

	opt->flags |= ACCESSED;
	if (is_yes_option(opt->val))
	{
		opt->flags &= ~INVALID;
		*x = 1;
		return 1;
	}
	if (is_no_option(opt->val))
	{
		opt->flags &= ~INVALID;
		*x = 0;
		return 1;
	}
	opt->flags |= INVALID; // not a boolean
	return -1;
}

int
fz_lookup_option_float(fz_context *ctx, fz_options *options, const char *key, float *x)
{
	fz_option *opt = lookup_option(ctx, options, key);
	float v;
	char *tail;
	if (!opt)
		return 0;

	opt->flags |= ACCESSED;
	v = fz_strtof(opt->val, &tail);
	if (tail > opt->val && *tail == 0)
	{
		opt->flags &= ~INVALID;
		*x = v;
		return 1;
	}
	opt->flags |= INVALID; // not a float
	return -1;
}

int
fz_lookup_option_integer(fz_context *ctx, fz_options *options, const char *key, int *x)
{
	fz_option *opt = lookup_option(ctx, options, key);
	char *tail;
	int v;
	if (!opt)
		return 0;

	opt->flags |= ACCESSED;
	v = strtol(opt->val, &tail, 10);
	if (tail > opt->val && *tail == 0)
	{
		opt->flags &= ~INVALID;
		*x = v;
		return 1;
	}
	opt->flags |= INVALID; // not an integer
	return -1;
}

int
fz_lookup_option_unsigned(fz_context *ctx, fz_options *options, const char *key, unsigned int *x)
{
	fz_option *opt = lookup_option(ctx, options, key);
	char *tail;
	int v;
	if (!opt)
		return 0;

	opt->flags |= ACCESSED;
	v = strtoul(opt->val, &tail, 10);
	if (tail > opt->val && *tail == 0)
	{
		opt->flags &= ~INVALID;
		*x = v;
		return 1;
	}
	opt->flags |= INVALID; // not an integer
	return -1;
}

int
fz_lookup_option_enum(fz_context *ctx, fz_options *options, const char *key, int *x,
	const fz_option_enums *enum_list)
{
	fz_option *opt = lookup_option(ctx, options, key);
	int i;
	if (!opt)
		return 0;

	opt->flags |= ACCESSED;
	for (i = 0; enum_list[i].key; ++i)
		if (!strcmp(enum_list[i].key, opt->val))
		{
			opt->flags &= ~INVALID;
			*x = enum_list[i].val;
			return 1;
		}
	*x = enum_list[i].val;
	opt->flags |= INVALID; // not in the list of valid values
	return -1;
}

/**
	If any options are used as inappropriate types, throw on them.
*/
void fz_validate_options(fz_context *ctx, fz_options *options, const char *prefix)
{
	fz_option *opt;
	int invalid = 0;

	if (!options)
		return;

	for (opt = options->head; opt; opt = opt->next)
	{
		if (opt->flags & INVALID)
		{
			opt->flags |= NOTIFIED; /* So check_unused_options() does not throw again. */
			invalid = 1;
			if (opt->val && opt->val[0])
				fz_warn(ctx, "invalid %s option: %s=%s", prefix, opt->key, opt->val);
			else
				fz_warn(ctx, "invalid %s option: %s", prefix, opt->key);
		}
	}
	/* Just throw once at the end so that all invalid options are reported, rather than just the first. */
	if (invalid)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid %s options found", prefix);
}

static int check_unused_options(fz_context *ctx, fz_options *options, const char *prefix)
{
	fz_option *opt;
	int unused = 0;
	int invalid = 0;
	if (!options)
		return 0;

	for (opt = options->head; opt; opt = opt->next)
	{
		if ((opt->flags & INVALID) && (opt->flags & NOTIFIED) == 0)
		{
			opt->flags |= NOTIFIED;
			invalid = 1;
			if (prefix && prefix[0])
				fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid %s option: %s=%s", prefix, opt->key, opt->val);
			else
				fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid option: %s=%s", opt->key, opt->val);
		}
		if (!(opt->flags & ACCESSED))
		{
			unused = 1;
			if (prefix && prefix[0])
				fz_warn(ctx, "unknown %s option: %s=%s", prefix, opt->key, opt->val);
			else
				fz_warn(ctx, "unknown option: %s=%s", opt->key, opt->val);
		}
	}
	if (invalid)
	{
		if (prefix && prefix[0])
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid %s options found", prefix);
		else
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid options found");
	}

	return unused;
}

void fz_throw_on_unused_options(fz_context *ctx, fz_options *options, const char *prefix)
{
	if (check_unused_options(ctx, options, prefix))
	{
		if (prefix && prefix[0])
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unused %s arguments found", prefix);
		else
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unused arguments found");
	}
}

void fz_warn_on_unused_options(fz_context *ctx, fz_options *options, const char *prefix)
{
	if (check_unused_options(ctx, options, prefix))
	{
		if (prefix && prefix[0])
			fz_warn(ctx, "Unused %s arguments found", prefix);
		else
			fz_warn(ctx, "Unused arguments found");
	}
}

int fz_count_options(fz_context *ctx, fz_options *options)
{
	fz_option *opt;
	int n = 0;
	if (options)
		for (opt = options->head; opt; opt = opt->next)
			++n;
	return n;
}

const char *fz_get_option_by_index(fz_context *ctx, fz_options *options, int i, const char **val)
{
	fz_option *opt;
	int n = 0;
	if (options)
	{
		for (opt = options->head; opt; opt = opt->next)
		{
			if (i == n)
				return *val = opt->val, opt->key;
			++n;
		}
	}
	fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid option index");
}

void fz_access_option_by_index(fz_context *ctx, fz_options *options, int i)
{
	fz_option *opt;
	int n = 0;
	if (options)
	{
		for (opt = options->head; opt; opt = opt->next)
		{
			if (i == n)
			{
				opt->flags |= ACCESSED;
				return;
			}
		}
	}
	fz_throw(ctx, FZ_ERROR_ARGUMENT, "Invalid option index");
}
