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

#ifndef MUPDF_FITZ_JSON_H
#define MUPDF_FITZ_JSON_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/pool.h"
#include "mupdf/fitz/buffer.h"

/* JSON parser */

enum {
	FZ_JSON_NULL,
	FZ_JSON_TRUE,
	FZ_JSON_FALSE,
	FZ_JSON_NUMBER,
	FZ_JSON_STRING,
	FZ_JSON_ARRAY,
	FZ_JSON_OBJECT
};

typedef struct fz_json fz_json;
typedef struct fz_json_array fz_json_array;
typedef struct fz_json_object fz_json_object;

struct fz_json
{
	int type;
	union {
		double number;
		const char *string;
		fz_json_array *array;
		fz_json_object *object;
	} u;
};

struct fz_json_array
{
	fz_json *value;
	fz_json_array *next;
};

struct fz_json_object
{
	const char *key;
	fz_json *value;
	fz_json_object *next;
};

fz_json *fz_parse_json(fz_context *ctx, fz_pool *pool, const char *s);

void fz_append_json(fz_context *ctx, fz_buffer *buf, fz_json *value);
void fz_write_json(fz_context *ctx, fz_output *out, fz_json *value);

fz_json *fz_json_new_null(fz_context *ctx, fz_pool *pool);
fz_json *fz_json_new_boolean(fz_context *ctx, fz_pool *pool, int x);
fz_json *fz_json_new_number(fz_context *ctx, fz_pool *pool, double number);
fz_json *fz_json_new_string(fz_context *ctx, fz_pool *pool, const char *string);
fz_json *fz_json_new_array(fz_context *ctx, fz_pool *pool);
fz_json *fz_json_new_object(fz_context *ctx, fz_pool *pool);
void fz_json_array_push(fz_context *ctx, fz_pool *pool, fz_json *array, fz_json *item);
void fz_json_object_set(fz_context *ctx, fz_pool *pool, fz_json *object, const char *key, fz_json *item);

int fz_json_is_null(fz_context *ctx, fz_json *json);
int fz_json_is_boolean(fz_context *ctx, fz_json *json);
int fz_json_is_number(fz_context *ctx, fz_json *json);
int fz_json_is_string(fz_context *ctx, fz_json *json);
int fz_json_is_array(fz_context *ctx, fz_json *json);
int fz_json_is_object(fz_context *ctx, fz_json *json);
int fz_json_to_boolean(fz_context *ctx, fz_json *json);
double fz_json_to_number(fz_context *ctx, fz_json *json);
const char *fz_json_to_string(fz_context *ctx, fz_json *json);
int fz_json_array_length(fz_context *ctx, fz_json *array);
fz_json *fz_json_array_get(fz_context *ctx, fz_json *array, int ix);
fz_json *fz_json_object_get(fz_context *ctx, fz_json *object, const char *key);

#endif
