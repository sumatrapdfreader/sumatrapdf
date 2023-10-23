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

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

fz_output *fz_new_log_for_module(fz_context *ctx, const char *module)
{
	char text[256];
	char *s = NULL;

	if (module)
	{
		fz_snprintf(text, sizeof(text), "FZ_LOG_FILE_%s", module);
		s = getenv(text);
	}
	if (s == NULL)
		s = getenv("FZ_LOG_FILE");
	if (s == NULL)
		s = "fitz_log.txt";
	return fz_new_output_with_path(ctx, s, 1);
}

void fz_log(fz_context *ctx, const char *fmt, ...)
{
	fz_output *out;
	va_list args;
	va_start(args, fmt);
	fz_try(ctx)
	{
		out = fz_new_log_for_module(ctx, NULL);
		fz_write_vprintf(ctx, out, fmt, args);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		va_end(args);
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void fz_log_module(fz_context *ctx, const char *module, const char *fmt, ...)
{
	fz_output *out;
	va_list args;
	va_start(args, fmt);
	fz_try(ctx)
	{
		out = fz_new_log_for_module(ctx, module);
		if (module)
			fz_write_printf(ctx, out, "%s\t", module);
		fz_write_vprintf(ctx, out, fmt, args);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		va_end(args);
		fz_drop_output(ctx, out);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}
