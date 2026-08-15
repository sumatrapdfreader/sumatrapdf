// Copyright (C) 2004-2025 Artifex Software, Inc.
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

#include <string.h>
#include <stdlib.h>
#include <limits.h>

/* JSON parse */

struct json_parser {
	fz_context *ctx;
	fz_pool *pool;
	const char *s;
};

static fz_json *json_parse_element(struct json_parser *pp);

static void json_error(struct json_parser *pp, const char *error)
{
	fz_throw(pp->ctx, FZ_ERROR_SYNTAX, "%s in JSON", error);
}

static fz_json *json_new_value(fz_context *ctx, fz_pool *pool, int type)
{
	fz_json *val = fz_pool_alloc(ctx, pool, sizeof(fz_json));
	val->type = type;
	return val;
}

static fz_json_array *json_new_array(fz_context *ctx, fz_pool *pool, fz_json *value)
{
	fz_json_array *array = fz_pool_alloc(ctx, pool, sizeof(fz_json_array));
	array->value = value;
	array->next = NULL;
	return array;
}

static fz_json_object *json_new_object(fz_context *ctx, fz_pool *pool, const char *key, fz_json *value)
{
	fz_json_object *object = fz_pool_alloc(ctx, pool, sizeof(fz_json_object));
	object->key = key;
	object->value = value;
	object->next = NULL;
	return object;
}

static int json_accept(struct json_parser *pp, int c)
{
	if (*pp->s == c)
	{
		pp->s++;
		return 1;
	}
	return 0;
}

static int json_accept_range(struct json_parser *pp, int a, int b)
{
	if (*pp->s >= a && *pp->s <= b)
	{
		pp->s++;
		return 1;
	}
	return 0;
}

static void json_expect_range(struct json_parser *pp, int a, int b)
{
	if (!json_accept_range(pp, a, b))
		fz_throw(pp->ctx, FZ_ERROR_SYNTAX, "expected '%c'-'%c' in JSON", a, b);
}

static void json_expect(struct json_parser *pp, int c)
{
	if (!json_accept(pp, c))
		fz_throw(pp->ctx, FZ_ERROR_SYNTAX, "expected '%c' in JSON", c);
}

static void json_parse_ws(struct json_parser *pp)
{
	int c = *pp->s;
	while (c == 0x0a || c == 0x0d || c == 0x09 || c == 0x20)
		c = *(++pp->s);
}

static int unhex(int c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 0xA;
	if (c >= 'A' && c <= 'F') return c - 'A' + 0xA;
	return 0;
}

static int json_expect_hex(struct json_parser *pp)
{
	int c = *pp->s++;
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 0xA;
	if (c >= 'A' && c <= 'F') return c - 'A' + 0xA;
	json_error(pp, "invalid unicode escape sequence");
	return 0;
}

static const char *
json_unescape_string(struct json_parser *pp, const char *s, const char *end, int n)
{
	char *str, *p;
	int x;
	str = p = fz_pool_alloc(pp->ctx, pp->pool, n + 1);
	while (s < end)
	{
		if (*s == '\\')
		{
			s++;
			switch (*s++)
			{
			case 'u':
				x = unhex(*s++) << 12;
				x |= unhex(*s++) << 8;
				x |= unhex(*s++) << 4;
				x |= unhex(*s++);
				p += fz_runetochar(p, x);
				break;
			case '\\': *p++ = '\\'; break;
			case '/': *p++ = '/'; break;
			case 'b': *p++ = '\b'; break;
			case 'f': *p++ = '\f'; break;
			case 'n': *p++ = '\n'; break;
			case 'r': *p++ = '\r'; break;
			case 't': *p++ = '\t'; break;
			}
		}
		else
		{
			*p++ = *s++;
		}
	}
	*p = 0;
	return str;
}

static const char *
json_parse_string(struct json_parser *pp)
{
	const char *mark;
	int n, c, x;

	json_expect(pp, '"');
	mark = pp->s;
	n = 0;

	for (;;)
	{
		c = (unsigned char) *pp->s++;
		if (c < 0x20)
			json_error(pp, "bad control character in string literal");
		if (c == '"')
			break;
		if (c == '\\')
		{
			c = *pp->s++;
			if (c == 'u')
			{
				x = json_expect_hex(pp) << 12;
				x |= json_expect_hex(pp) << 8;
				x |= json_expect_hex(pp) << 4;
				x |= json_expect_hex(pp);
				n += fz_runelen(x);
			}
			else if (c == '"' || c == '\\' || c == '/' || c == 'b' || c == 'f' || c == 'n' || c == 'r' || c == 't')
				n += 1;
			else
				json_error(pp, "bad escaped character");
		}
		else
		{
			n += 1;
		}
	}

	return json_unescape_string(pp, mark, pp->s - 1, n);
}

static fz_json *
json_parse_number(struct json_parser *pp)
{
	fz_json *val;
	const char *mark = pp->s;

	json_accept(pp, '-');
	if (json_accept(pp, '0'))
	{
	}
	else
	{
		json_expect_range(pp, '1', '9');
		while (json_accept_range(pp, '0', '9'))
			;
	}

	// fraction
	if (json_accept(pp, '.'))
	{
		json_expect_range(pp, '0', '9');
		while (json_accept_range(pp, '0', '9'))
			;
	}

	// exponent
	if (json_accept(pp, 'e') || json_accept(pp, 'E'))
	{
		if (json_accept(pp, '-') || json_accept(pp, '+'))
			;
		json_expect_range(pp, '0', '9');
		while (json_accept_range(pp, '0', '9'))
			;
	}

	val = json_new_value(pp->ctx, pp->pool, FZ_JSON_NUMBER);
	val->u.number = fz_atof(mark);

	return val;
}

static fz_json *
json_parse_object(struct json_parser *pp)
{
	fz_json *obj;
	fz_json_object **tail;
	const char *key;
	fz_json *val;

	json_expect(pp, '{');

	obj = json_new_value(pp->ctx, pp->pool, FZ_JSON_OBJECT);
	tail = &obj->u.object;

	json_parse_ws(pp);
	if (json_accept(pp, '}'))
		return obj;

	for (;;)
	{
		json_parse_ws(pp);
		key = json_parse_string(pp);
		json_parse_ws(pp);
		json_expect(pp, ':');
		val = json_parse_element(pp);
		*tail = json_new_object(pp->ctx, pp->pool, key, val);
		tail = &(*tail)->next;
		if (json_accept(pp, '}'))
			break;
		json_expect(pp, ',');
	}

	return obj;
}

static fz_json *
json_parse_array(struct json_parser *pp)
{
	fz_json *arr;
	fz_json_array **tail;
	fz_json *val;

	json_expect(pp, '[');

	arr = json_new_value(pp->ctx, pp->pool, FZ_JSON_ARRAY);
	tail = &arr->u.array;

	json_parse_ws(pp);
	if (json_accept(pp, ']'))
		return arr;

	for (;;)
	{
		val = json_parse_element(pp);
		*tail = json_new_array(pp->ctx, pp->pool, val);
		tail = &(*tail)->next;
		if (json_accept(pp, ']'))
			break;
		json_expect(pp, ',');
	}

	return arr;
}

static fz_json *
json_parse_value(struct json_parser *pp)
{
	fz_json *val;
	int lookahead = *pp->s;
	if (lookahead == '{')
		return json_parse_object(pp);
	if (lookahead == '[')
		return json_parse_array(pp);
	if (lookahead == '"')
	{
		val = json_new_value(pp->ctx, pp->pool, FZ_JSON_STRING);
		val->u.string = json_parse_string(pp);
		return val;
	}
	if (lookahead == '-' || (lookahead >= '0' && lookahead <= '9'))
		return json_parse_number(pp);
	if (json_accept(pp, 'n'))
	{
		json_expect(pp, 'u');
		json_expect(pp, 'l');
		json_expect(pp, 'l');
		return json_new_value(pp->ctx, pp->pool, FZ_JSON_NULL);
	}
	if (json_accept(pp, 't'))
	{
		json_expect(pp, 'r');
		json_expect(pp, 'u');
		json_expect(pp, 'e');
		return json_new_value(pp->ctx, pp->pool, FZ_JSON_TRUE);
	}
	if (json_accept(pp, 'f'))
	{
		json_expect(pp, 'a');
		json_expect(pp, 'l');
		json_expect(pp, 's');
		json_expect(pp, 'e');
		return json_new_value(pp->ctx, pp->pool, FZ_JSON_FALSE);
	}
	json_error(pp, "unexpected token");
	return NULL;
}

static fz_json *
json_parse_element(struct json_parser *pp)
{
	fz_json *result;
	json_parse_ws(pp);
	result = json_parse_value(pp);
	json_parse_ws(pp);
	return result;
}

fz_json *
fz_parse_json(fz_context *ctx, fz_pool *pool, const char *s)
{
	struct json_parser p = { ctx, pool, s };
	fz_json *result = json_parse_element(&p);
	json_expect(&p, 0);
	return result;
}

/* JSON stringify */

static void
append_json_string(fz_context *ctx, fz_buffer *out, const char *s)
{
	int c;
	fz_append_byte(ctx, out, '"');
	while (*s)
	{
		s += fz_chartorune(&c, s);
		if (c < 20 || c == '"' || c == '\\' || c >= 127)
		{
			fz_append_byte(ctx, out, '\\');
			switch (c)
			{
			case '"': fz_append_byte(ctx, out, '"'); break;
			case '\\': fz_append_byte(ctx, out, '\\'); break;
			case '\n': fz_append_byte(ctx, out, 'n'); break;
			case '\r': fz_append_byte(ctx, out, 'r'); break;
			case '\t': fz_append_byte(ctx, out, 't'); break;
			default: fz_append_printf(ctx, out, "u%04x", c); break;
			}
		}
		else
		{
			fz_append_byte(ctx, out, c);
		}
	}
	fz_append_byte(ctx, out, '"');
}

static void
write_json_string(fz_context *ctx, fz_output *out, const char *s)
{
	int c;
	fz_write_byte(ctx, out, '"');
	while (*s)
	{
		s += fz_chartorune(&c, s);
		if (c < 20 || c == '"' || c == '\\' || c >= 127)
		{
			fz_write_byte(ctx, out, '\\');
			switch (c)
			{
			case '"': fz_write_byte(ctx, out, '"'); break;
			case '\\': fz_write_byte(ctx, out, '\\'); break;
			case '\n': fz_write_byte(ctx, out, 'n'); break;
			case '\r': fz_write_byte(ctx, out, 'r'); break;
			case '\t': fz_write_byte(ctx, out, 't'); break;
			default: fz_write_printf(ctx, out, "u%04x", c); break;
			}
		}
		else
		{
			fz_write_byte(ctx, out, c);
		}
	}
	fz_write_byte(ctx, out, '"');
}

static const char *format_json_integer(char *out, int v)
{
	char buf[32], *s = out;
	unsigned int a;
	int i = 0;
	if (v < 0) {
		a = -v;
		*s++ = '-';
	} else {
		a = v;
	}
	while (a) {
		buf[i++] = (a % 10) + '0';
		a /= 10;
	}
	if (i == 0)
		buf[i++] = '0';
	while (i > 0)
		*s++ = buf[--i];
	*s = 0;
	return out;
}

static const char *format_json_exponent(char *p, int e)
{
	*p++ = 'e';
	if (e < 0)
	{
		*p++ = '-';
		return format_json_integer(p, -e);
	}
	else
	{
		*p++ = '+';
		return format_json_integer(p, e);
	}
}

static const char *format_json_number(char buf[32], double f)
{
	char digits[32], *p = buf, *s = digits;
	int exp, ndigits, point;

	if (f == 0) return "0";
	if (isnan(f)) return "null";
	if (isinf(f)) return "null";

	/* Fast case for integers. This only works assuming all integers can be
	 * exactly represented by a float. This is true for 32-bit integers and
	 * 64-bit floats. */
	if (f >= INT_MIN && f <= INT_MAX) {
		int i = (int)f;
		if ((double)i == f)
			return format_json_integer(buf, i);
	}

	// TODO: use double precision grisu algorithm!
	ndigits = fz_grisu(f, digits, &exp);
	point = ndigits + exp;

	if (signbit(f))
		*p++ = '-';

	if (point < -5 || point > 21) {
		*p++ = *s++;
		if (ndigits > 1) {
			int n = ndigits - 1;
			*p++ = '.';
			while (n--)
				*p++ = *s++;
		}
		format_json_exponent(p, point - 1);
	}

	else if (point <= 0) {
		*p++ = '0';
		*p++ = '.';
		while (point++ < 0)
			*p++ = '0';
		while (ndigits-- > 0)
			*p++ = *s++;
		*p = 0;
	}

	else {
		while (ndigits-- > 0) {
			*p++ = *s++;
			if (--point == 0 && ndigits > 0)
				*p++ = '.';
		}
		while (point-- > 0)
			*p++ = '0';
		*p = 0;
	}

	return buf;
}

void
fz_append_json(fz_context *ctx, fz_buffer *out, fz_json *value)
{
	fz_json_array *arr;
	fz_json_object *obj;
	char buf[40];
	switch (value->type)
	{
	case FZ_JSON_NULL:
		fz_append_string(ctx, out, "null");
		break;
	case FZ_JSON_TRUE:
		fz_append_string(ctx, out, "true");
		break;
	case FZ_JSON_FALSE:
		fz_append_string(ctx, out, "false");
		break;
	case FZ_JSON_NUMBER:
		fz_append_string(ctx, out, format_json_number(buf, value->u.number));
		break;
	case FZ_JSON_STRING:
		append_json_string(ctx, out, value->u.string);
		break;
	case FZ_JSON_ARRAY:
		fz_append_byte(ctx, out, '[');
		for (arr = value->u.array; arr; arr = arr->next)
		{
			if (arr != value->u.array)
				fz_append_byte(ctx, out, ',');
			fz_append_json(ctx, out, arr->value);
		}
		fz_append_byte(ctx, out, ']');
		break;
	case FZ_JSON_OBJECT:
		fz_append_byte(ctx, out, '{');
		for (obj = value->u.object; obj; obj = obj->next)
		{
			if (obj != value->u.object)
				fz_append_byte(ctx, out, ',');
			append_json_string(ctx, out, obj->key);
			fz_append_byte(ctx, out, ':');
			fz_append_json(ctx, out, obj->value);
		}
		fz_append_byte(ctx, out, '}');
		break;
	}
}

void
fz_write_json(fz_context *ctx, fz_output *out, fz_json *value)
{
	fz_json_array *arr;
	fz_json_object *obj;
	char buf[40];
	switch (value->type)
	{
	case FZ_JSON_NULL:
		fz_write_string(ctx, out, "null");
		break;
	case FZ_JSON_TRUE:
		fz_write_string(ctx, out, "true");
		break;
	case FZ_JSON_FALSE:
		fz_write_string(ctx, out, "false");
		break;
	case FZ_JSON_NUMBER:
		fz_write_string(ctx, out, format_json_number(buf, value->u.number));
		break;
	case FZ_JSON_STRING:
		write_json_string(ctx, out, value->u.string);
		break;
	case FZ_JSON_ARRAY:
		fz_write_byte(ctx, out, '[');
		for (arr = value->u.array; arr; arr = arr->next)
		{
			if (arr != value->u.array)
				fz_write_byte(ctx, out, ',');
			fz_write_json(ctx, out, arr->value);
		}
		fz_write_byte(ctx, out, ']');
		break;
	case FZ_JSON_OBJECT:
		fz_write_byte(ctx, out, '{');
		for (obj = value->u.object; obj; obj = obj->next)
		{
			if (obj != value->u.object)
				fz_write_byte(ctx, out, ',');
			write_json_string(ctx, out, obj->key);
			fz_write_byte(ctx, out, ':');
			fz_write_json(ctx, out, obj->value);
		}
		fz_write_byte(ctx, out, '}');
		break;
	}
}

/* JSON accessors */

int fz_json_is_null(fz_context *ctx, fz_json *json)
{
	return json && json->type == FZ_JSON_NULL;
}

int fz_json_is_boolean(fz_context *ctx, fz_json *json)
{
	return json && (json->type == FZ_JSON_TRUE || json->type == FZ_JSON_FALSE);
}

int fz_json_is_number(fz_context *ctx, fz_json *json)
{
	return json && json->type == FZ_JSON_NUMBER;
}

int fz_json_is_string(fz_context *ctx, fz_json *json)
{
	return json && json->type == FZ_JSON_STRING;
}

int fz_json_is_array(fz_context *ctx, fz_json *json)
{
	return json && json->type == FZ_JSON_ARRAY;
}

int fz_json_is_object(fz_context *ctx, fz_json *json)
{
	return json && json->type == FZ_JSON_OBJECT;
}

int fz_json_to_boolean(fz_context *ctx, fz_json *json)
{
	return json && json->type == FZ_JSON_TRUE;
}

double fz_json_to_number(fz_context *ctx, fz_json *json)
{
	if (json && json->type == FZ_JSON_NUMBER)
		return json->u.number;
	return 0;
}

const char *fz_json_to_string(fz_context *ctx, fz_json *json)
{
	if (json && json->type == FZ_JSON_STRING)
		return json->u.string;
	return "";
}

int fz_json_array_length(fz_context *ctx, fz_json *array)
{
	fz_json_array *entry;
	int n = 0;
	if (array->type != FZ_JSON_ARRAY)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not an array");
	for (entry = array->u.array; entry; entry = entry->next)
		++n;
	return n;
}

fz_json *fz_json_array_get(fz_context *ctx, fz_json *array, int ix)
{
	fz_json_array *entry;
	int n = 0;
	if (array->type != FZ_JSON_ARRAY)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not an array");
	for (entry = array->u.array; entry; entry = entry->next)
	{
		if (n == ix)
			return entry->value;
		++n;
	}
	return NULL;
}

fz_json *fz_json_object_get(fz_context *ctx, fz_json *object, const char *key)
{
	fz_json_object *entry;
	if (object->type != FZ_JSON_OBJECT)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not an object");
	for (entry = object->u.object; entry; entry = entry->next)
		if (!strcmp(entry->key, key))
			return entry->value;
	return NULL;
}

/* JSON build objects */

fz_json *fz_json_new_object(fz_context *ctx, fz_pool *pool)
{
	return json_new_value(ctx, pool, FZ_JSON_OBJECT);
}

fz_json *fz_json_new_array(fz_context *ctx, fz_pool *pool)
{
	return json_new_value(ctx, pool, FZ_JSON_ARRAY);
}

void fz_json_array_push(fz_context *ctx, fz_pool *pool, fz_json *array, fz_json *item)
{
	fz_json_array **tail;
	if (array->type != FZ_JSON_ARRAY)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not an array");
	tail = &array->u.array;
	while (*tail != NULL)
		tail = &(*tail)->next;
	*tail = json_new_array(ctx, pool, item);
}

void fz_json_object_set(fz_context *ctx, fz_pool *pool, fz_json *object, const char *key, fz_json *item)
{
	fz_json_object **tail;
	if (object->type != FZ_JSON_OBJECT)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "not an object");
	tail = &object->u.object;
	while (*tail != NULL)
	{
		if (!strcmp((*tail)->key, key))
		{
			// replace old value!
			(*tail)->value = item;
			return;
		}
		tail = &(*tail)->next;
	}
	*tail = json_new_object(ctx, pool, fz_pool_strdup(ctx, pool, key), item);
}

fz_json *fz_json_new_number(fz_context *ctx, fz_pool *pool, double number)
{
	fz_json *val = json_new_value(ctx, pool, FZ_JSON_NUMBER);
	val->u.number = number;
	return val;
}

fz_json *fz_json_new_string(fz_context *ctx, fz_pool *pool, const char *string)
{
	fz_json *val = json_new_value(ctx, pool, FZ_JSON_STRING);
	val->u.string = fz_pool_strdup(ctx, pool, string);
	return val;
}

fz_json *fz_json_new_boolean(fz_context *ctx, fz_pool *pool, int x)
{
	if (x)
		return json_new_value(ctx, pool, FZ_JSON_TRUE);
	return json_new_value(ctx, pool, FZ_JSON_FALSE);
}

fz_json *fz_json_new_null(fz_context *ctx, fz_pool *pool)
{
	return json_new_value(ctx, pool, FZ_JSON_NULL);
}
