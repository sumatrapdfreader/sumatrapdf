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

#ifdef TRACK_USAGE

static track_usage_data_t *usage_head = NULL;

static void dump_usage(void)
{
	track_usage_data_t *u = usage_head;

	while (u)
	{
		fprintf(stderr, "USAGE: %s (%s:%d) %d calls\n",
			u->desc, u->function, u->line, u->count);
		u = u->next;
	}
}

void track_usage(track_usage_data_t *data, const char *function, int line, const char *desc)
{
	int c = data->count++;
	if (c == 0)
	{
		data->function = function;
		data->line = line;
		data->desc = desc;
		if (usage_head == NULL)
			atexit(dump_usage);
		data->next = usage_head;
		usage_head = data;
	}
}

#endif
