// Copyright (C) 2014-2026 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_REGEXP_H
#define MUPDF_FITZ_REGEXP_H

/* Flag to regcomp to enable case insensitive matching. */
#define REG_ICASE 1

/* Flag to regcomp to enable matching ^ and $ on newlines. */
#define REG_NEWLINE 2

/* Flag to regexec to indicate that the string does not start at the beginning of a line. */
#define REG_NOTBOL 4

/* Flag to regexec to detect runaway backtracking. */
#define REG_RUNAWAY 8

/*
	Maximum number of sub expression that can be captured.
	If you redefine REG_MAXSUB, you must make sure both the calling
	code and the regexp.c compilation unit use the same value!
*/
#ifndef REG_MAXSUB
#define REG_MAXSUB 16
#endif

/* Opaque datatype for a compiled regular expression program. */
typedef struct fz_regex fz_regex;

/* Struct holding the results of a successful pattern match. */
struct fz_regmatch {
	int nsub;
	struct {
		const char *sp;
		const char *ep;
	} sub[REG_MAXSUB];
};

/* Compile regular expression program. */
struct fz_regex * fz_regcomp(fz_context *ctx, const char *pattern, int cflags);

/* Match regular expression program against a text string. */
int fz_regexec(fz_context *ctx, struct fz_regex *prog, const char *string, struct fz_regmatch *sub, int eflags);

/* Free regular exppression program. */
void fz_regfree(fz_context *ctx, struct fz_regex *prog);

#endif
