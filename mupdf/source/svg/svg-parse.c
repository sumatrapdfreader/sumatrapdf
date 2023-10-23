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

#include "mupdf/fitz.h"
#include "svg-imp.h"

#include <string.h>
#include <math.h>

int svg_is_whitespace_or_comma(int c)
{
	 return (c == 0x20) || (c == 0x9) || (c == 0xD) || (c == 0xA) || (c == ',');
}

int svg_is_whitespace(int c)
{
	return (c == 0x20) || (c == 0x9) || (c == 0xD) || (c == 0xA);
}

int svg_is_alpha(int c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int svg_is_digit(int c)
{
	return (c >= '0' && c <= '9') ||
		(c == 'e') || (c == 'E') ||
		(c == '+') || (c == '-') || (c == '.');
}

const char *
svg_lex_number(float *fp, const char *ss)
{
	const char *s = ss;
	if (*s == '+' || *s == '-')
		++s;
	while (*s >= '0' && *s <= '9')
		++s;
	if (*s == '.') {
		++s;
		while (*s >= '0' && *s <= '9')
			++s;
	}
	if (*s == 'e' || *s == 'E') {
		++s;
		if (*s == '+' || *s == '-')
			++s;
		while (*s >= '0' && *s <= '9')
			++s;
	}
	*fp = fz_atof(ss);
	return s;
}

float
svg_parse_number(const char *str, float min, float max, float inherit)
{
	float x;
	if (!strcmp(str, "inherit"))
		return inherit;
	x = fz_atof(str);
	if (x < min) return min;
	if (x > max) return max;
	return x;
}

float
svg_parse_length(const char *str, float percent, float font_size)
{
	char *end;
	float val;

	val = fz_strtof(str, &end);
	if (end == str)
		return 0; /* failed */

	if (!strcmp(end, "px")) return val;

	if (!strcmp(end, "pt")) return val * 1.0f;
	if (!strcmp(end, "pc")) return val * 12.0f;
	if (!strcmp(end, "mm")) return val * 2.83464567f;
	if (!strcmp(end, "cm")) return val * 28.3464567f;
	if (!strcmp(end, "in")) return val * 72.0f;

	if (!strcmp(end, "em")) return val * font_size;
	if (!strcmp(end, "ex")) return val * font_size * 0.5f;

	if (!strcmp(end, "%"))
		return val * percent * 0.01f;

	if (end[0] == 0)
		return val;

	return 0;
}

/* Return angle in degrees */
float
svg_parse_angle(const char *str)
{
	char *end;
	float val;

	val = fz_strtof(str, &end);
	if (end == str)
		return 0; /* failed */

	if (!strcmp(end, "deg"))
		return val;

	if (!strcmp(end, "grad"))
		return val * 0.9f;

	if (!strcmp(end, "rad"))
		return val * FZ_RADIAN;

	return val;
}

/* Coordinate transformations */
fz_matrix
svg_parse_transform(fz_context *ctx, svg_document *doc, const char *str, fz_matrix transform)
{
	char keyword[20];
	int keywordlen;
	float args[6];
	int nargs;

	nargs = 0;
	keywordlen = 0;

	while (*str)
	{
		while (svg_is_whitespace_or_comma(*str))
			str ++;
		if (*str == 0)
			break;

		/*
		 * Parse keyword and opening parenthesis.
		 */

		keywordlen = 0;
		while (svg_is_alpha(*str) && keywordlen < (int)sizeof(keyword) - 1)
			keyword[keywordlen++] = *str++;
		keyword[keywordlen] = 0;

		if (keywordlen == 0)
			fz_throw(ctx, FZ_ERROR_SYNTAX, "expected keyword in transform attribute");

		while (svg_is_whitespace(*str))
			str ++;

		if (*str != '(')
			fz_throw(ctx, FZ_ERROR_SYNTAX, "expected opening parenthesis in transform attribute");
		str ++;

		/*
		 * Parse list of numbers until closing parenthesis
		 */

		nargs = 0;
		while (*str && *str != ')' && nargs < 6)
		{
			while (svg_is_whitespace_or_comma(*str))
				str ++;
			if (svg_is_digit(*str))
				str = svg_lex_number(&args[nargs++], str);
			else
				break;
		}

		if (*str != ')')
			fz_throw(ctx, FZ_ERROR_SYNTAX, "expected closing parenthesis in transform attribute");
		str ++;

		/*
		 * Execute the transform.
		 */

		if (!strcmp(keyword, "matrix"))
		{
			if (nargs != 6)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "wrong number of arguments to matrix(): %d", nargs);
			transform = fz_concat(fz_make_matrix(args[0], args[1], args[2], args[3], args[4], args[5]), transform);
		}

		else if (!strcmp(keyword, "translate"))
		{
			if (nargs == 1)
				transform = fz_concat(fz_translate(args[0], 0), transform);
			else if (nargs == 2)
				transform = fz_concat(fz_translate(args[0], args[1]), transform);
			else
				fz_throw(ctx, FZ_ERROR_SYNTAX, "wrong number of arguments to translate(): %d", nargs);
		}

		else if (!strcmp(keyword, "scale"))
		{
			if (nargs == 1)
				transform = fz_concat(fz_scale(args[0], args[0]), transform);
			else if (nargs == 2)
				transform = fz_concat(fz_scale(args[0], args[1]), transform);
			else
				fz_throw(ctx, FZ_ERROR_SYNTAX, "wrong number of arguments to scale(): %d", nargs);
		}

		else if (!strcmp(keyword, "rotate"))
		{
			if (nargs == 1)
				transform = fz_concat(fz_rotate(args[0]), transform);
			else if (nargs == 3)
			{
				transform = fz_concat(fz_translate(args[1], args[2]), transform);
				transform = fz_concat(fz_rotate(args[0]), transform);
				transform = fz_concat(fz_translate(-args[1], -args[2]), transform);
			}
			else
				fz_throw(ctx, FZ_ERROR_SYNTAX, "wrong number of arguments to rotate(): %d", nargs);
		}

		else if (!strcmp(keyword, "skewX"))
		{
			if (nargs != 1)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "wrong number of arguments to skewX(): %d", nargs);
			transform = fz_concat(fz_make_matrix(1, 0, tanf(args[0] * FZ_DEGREE), 1, 0, 0), transform);
		}

		else if (!strcmp(keyword, "skewY"))
		{
			if (nargs != 1)
				fz_throw(ctx, FZ_ERROR_SYNTAX, "wrong number of arguments to skewY(): %d", nargs);
			transform = fz_concat(fz_make_matrix(1, tanf(args[0] * FZ_DEGREE), 0, 1, 0, 0), transform);
		}

		else
		{
			fz_throw(ctx, FZ_ERROR_SYNTAX, "unknown transform function: %s", keyword);
		}
	}

	return transform;
}

float
svg_parse_number_from_style(fz_context *ctx, svg_document *doc, const char *style, const char *att, float number)
{
	if (style)
	{
		char *end, *p = strstr(style, att);
		if (p)
		{
			size_t n = strlen(att);
			if (p[n] == ':')
			{
				p += n + 1;
				while (*p && svg_is_whitespace(*p))
					++p;
				number = fz_strtof(p, &end);
				if (end[0] == 'i' && end[1] == 'n') return number * 72;
				if (end[0] == 'c' && end[1] == 'm') return number * 7200 / 254;
				if (end[0] == 'm' && end[1] == 'm') return number * 720 / 254;
				if (end[0] == 'p' && end[1] == 'c') return number * 12;
			}
		}
	}
	return number;
}

int
svg_parse_enum_from_style(fz_context *ctx, svg_document *doc, const char *style, const char *att,
	int ecount, const char *etable[], int value)
{
	char buf[100], *end, *p;
	int i;
	if (style)
	{
		p = strstr(style, att);
		if (p)
		{
			size_t n = strlen(att);
			if (p[n] == ':')
			{
				p += n + 1;
				while (*p && svg_is_whitespace(*p))
					++p;
				fz_strlcpy(buf, p, sizeof buf);
				end = strchr(buf, ';');
				if (end)
					*end = 0;
				for (i = 0; i < ecount; ++i)
					if (!strcmp(etable[i], buf))
						return i;
			}
		}
	}
	return value;
}

char *
svg_parse_string_from_style(fz_context *ctx, svg_document *doc, const char *style, const char *att,
	char *buf, int buf_size, const char *value)
{
	char *end, *p, quote;
	if (style)
	{
		p = strstr(style, att);
		if (p)
		{
			size_t n = strlen(att);
			if (p[n] == ':')
			{
				p += n + 1;
				while (*p && svg_is_whitespace(*p))
					++p;
				quote = *p;
				if (quote == '\'' || quote == '"')
				{
					fz_strlcpy(buf, p+1, buf_size);
					end = strchr(buf, quote);
				}
				else
				{
					fz_strlcpy(buf, p, buf_size);
					end = strchr(buf, ';');
				}
				if (end)
					*end = 0;
				return buf;
			}
		}
	}
	fz_strlcpy(buf, value, buf_size);
	return buf;
}
