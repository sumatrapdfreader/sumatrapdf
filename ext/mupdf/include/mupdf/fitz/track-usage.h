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

#ifndef TRACK_USAGE_H
#define TRACK_USAGE_H

#ifdef TRACK_USAGE

typedef struct track_usage_data {
	int count;
	const char *function;
	int line;
	const char *desc;
	struct track_usage_data *next;
} track_usage_data;

#define TRACK_LABEL(A) \
	do { \
		static track_usage_data USAGE_DATA = { 0 };\
		track_usage(&USAGE_DATA, __FILE__, __LINE__, A);\
	} while (0)

#define TRACK_FN() \
	do { \
		static track_usage_data USAGE_DATA = { 0 };\
		track_usage(&USAGE_DATA, __FILE__, __LINE__, __FUNCTION__);\
	} while (0)

void track_usage(track_usage_data *data, const char *function, int line, const char *desc);

#else

#define TRACK_LABEL(A) do { } while (0)
#define TRACK_FN() do { } while (0)

#endif

#endif
