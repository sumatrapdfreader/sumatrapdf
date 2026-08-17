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

#ifndef MUPDF_FITZ_LOG_H
#define MUPDF_FITZ_LOG_H

#include "mupdf/fitz/context.h"
#include "mupdf/fitz/output.h"

/**
	The functions in this file offer simple logging abilities.

	The default logfile is "fitz_log.txt". This can overridden by
	defining an environment variable "FZ_LOG_FILE", or module
	specific environment variables "FZ_LOG_FILE_<module>" (e.g.
	"FZ_LOG_FILE_STORE").

	Enable the following define(s) to enable built in debug logging
	from within the appropriate module(s).
*/

/* #define ENABLE_STORE_LOGGING */


/**
	Output a line to the log.
*/
void fz_log(fz_context *ctx, const char *fmt, ...);

/**
	Output a line to the log for a given module.
*/
void fz_log_module(fz_context *ctx, const char *module, const char *fmt, ...);

/**
	Internal function to actually do the opening of the logfile.

	Caller should close/drop the output when finished with it.
*/
fz_output *fz_new_log_for_module(fz_context *ctx, const char *module);

#endif
