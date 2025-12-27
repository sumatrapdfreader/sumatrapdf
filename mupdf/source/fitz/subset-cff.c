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

#include <math.h>

/*
	For the purposes of this code, and to save my tiny brain from
	overload, we will adopt the following notation:

	1) The PDF file contains bytes of data. These bytes are looked
	up in the MuPDF font handling to resolve to 'glyph ids' (gids).
	These account for all the different encodings etc in use,
	including the encoding table within the font.

	(For CIDFonts, Cid = Gid, and there is no encoding table).

	2) We are given the list of gids that are used in the document.

	Unlike for simple TTFs, we don't map these down to the bottom of the
	range, we just remove the definitions for them.

	For now, I'm leaving zero size charstrings for subsetted glyphs.
	This may need to be changed to be predefined charstrings that
	just set a zero width if this is illegal.

	Similarly, for now, we don't attempt to compact either the local
	or global subrs.
*/

/*
	In CFF files, we have:

	Charset: Maps from gid <-> glyph name

	Encoding: Maps from code <-> gid
		plus supplemental code -> glyph name (which must have been used already in the map)
*/


/* Contains 1-4, to tell us the size of offsets used. */
typedef uint8_t offsize_t;

typedef struct
{
	/* Index position and length in the original */
	uint32_t index_offset;
	uint32_t index_size;

	/* Fields read from the index */
	uint16_t count;
	offsize_t offsize;
	const uint8_t *offset; /* A pointer to the offset table, not to the data table! */

	/* The offset of the byte before the data. The offset of the first
	 * object is always 1. Add the offset of any given object to this
	 * and you get the offset within the block. */
	uint32_t data_offset;
} index_t;

typedef struct
{
	uint8_t scanned;
	uint16_t num;
} usage_t;

typedef struct
{
	int len;
	int max;
	usage_t *list;
} usage_list_t;

typedef struct
{
	uint8_t *base;
	size_t len;

	int symbolic;
	int is_cidfont;

	uint8_t major;
	uint8_t minor;
	uint8_t headersize;
	offsize_t offsize;
	offsize_t new_offsize;

	index_t name_index;
	index_t top_dict_index;
	index_t string_index;
	index_t global_index;
	index_t charstrings_index;
	index_t local_index;
	index_t fdarray_index;
	uint16_t gsubr_bias;
	uint16_t subr_bias;
	uint32_t top_dict_index_offset;
	uint32_t string_index_offset;
	uint32_t global_index_offset;
	uint32_t encoding_offset;
	uint32_t encoding_len;
	uint32_t charset_offset;
	uint32_t charset_len;
	uint32_t charstrings_index_offset;
	uint32_t private_offset;
	uint32_t private_len;
	uint32_t local_index_offset;
	uint32_t fdselect_offset;
	uint32_t fdselect_len;
	uint32_t fdarray_index_offset;
	uint32_t charstring_type;

	uint16_t unpacked_charset_len;
	uint16_t unpacked_charset_max;
	uint16_t *unpacked_charset;

	struct
	{
		fz_buffer *rewritten_dict;
		fz_buffer *rewritten_private;
		uint32_t offset;
		uint32_t len;
		uint32_t fixup;
		uint32_t local_index_offset;
		index_t local_index;
		usage_list_t local_usage;
		uint16_t subr_bias;
		fz_buffer *local_subset;
	} *fdarray;

	struct
	{
		uint32_t charset;
		uint32_t encoding;
		uint32_t charstrings;
		uint32_t privat;
		uint32_t fdselect;
		uint32_t fdarray;
	} top_dict_fixup_offsets;

	fz_buffer *charstrings_subset;
	fz_buffer *top_dict_subset;
	fz_buffer *private_subset;
	fz_buffer *local_subset;
	fz_buffer *global_subset;

	usage_list_t local_usage;
	usage_list_t global_usage;
	usage_list_t gids_to_keep;
	usage_list_t extra_gids_to_keep;

	uint16_t *gid_to_cid;
	uint8_t *gid_to_font;
} cff_t;

/* cid -> gid */
static const uint8_t standard_encoding[256] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
	17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
	33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64,
	65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
	81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
	0, 111, 112, 113, 114, 0, 115, 116, 117, 118, 119, 120, 121, 122, 0, 123,
	0, 124, 125, 126, 127, 128, 129, 130, 131, 0, 132, 133, 0, 134, 135, 136,
	137, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 138, 0, 139, 0, 0, 0, 0, 140, 141, 142, 143, 0, 0, 0, 0,
	0, 144, 0, 0, 0, 145, 0, 0, 146, 147, 148, 149, 0, 0, 0, 0
};

/* Simple functions for bigendian fetching/putting */

static uint32_t get16(const uint8_t *d)
{
	return (d[0]<<8)|d[1];
}

static void put32(uint8_t *d, uint32_t v)
{
	d[0] = v>>24;
	d[1] = v>>16;
	d[2] = v>>8;
	d[3] = v;
}

static void put16(uint8_t *d, uint32_t v)
{
	d[0] = v>>8;
	d[1] = v;
}

static void put8(uint8_t *d, uint32_t v)
{
	d[0] = v;
}

static uint32_t
get_offset(const uint8_t *d, offsize_t os)
{
	uint32_t v = *d++;
	if (os > 1)
		v = (v<<8) | *d++;;
	if (os > 2)
		v = (v<<8) | *d++;;
	if (os > 3)
		v = (v<<8) | *d++;;

	return v;
}

static void
put_offset(uint8_t *d, offsize_t os, uint32_t v)
{
	if (os > 3)
		d[3] = v, v >>= 8;
	if (os > 2)
		d[2] = v, v >>= 8;
	if (os > 1)
		d[1] = v, v >>= 8;
	d[0] = v;
}

static uint8_t
offsize_for_offset(uint32_t offset)
{
	if (offset < 256)
		return 1;
	if (offset < 65536)
		return 2;
	if (offset < (1<<24))
		return 3;
	return 4;
}

uint16_t
subr_bias(fz_context *ctx, cff_t *cff, uint16_t count)
{
	if (cff->charstring_type == 1)
		return 0;
	else if (count < 1240)
		return 107;
	else if (count < 33900)
		return 1131;
	else
		return 32768;
}

/* Index functions */

/* "Load" an index and check it for plausibility (no overflows etc) */
static uint32_t
index_load(fz_context *ctx, index_t *index, const uint8_t *base, uint32_t len, uint32_t offset)
{
	uint32_t data_offset, i, v, prev;
	offsize_t os;
	const uint8_t *data = base + offset;
	const uint8_t *data0 = data;

	/* Non-existent tables leave the index empty */
	if (offset == 0 || len == 0)
	{
		memset(index, 0, sizeof(*index));
		return 0;
	}

	index->index_offset = offset;

	if (offset >= len || len-offset < 2)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated index");

	index->count = get16(data);

	if (index->count == 0)
		return offset+2;

	if (offset + 4 >= len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated index");

	os = index->offsize = data[2];
	if (index->offsize < 1 || index->offsize > 4)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Illegal offsize");

	index->offset = data + 3;

	data_offset = 3 + (index->count+1) * os - 1;
	index->data_offset = data_offset + offset;

	if (data_offset > len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated index");

	data += 3;
	prev = get_offset(data, os);
	if (prev != 1)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Corrupt index");
	data += os;
	for (i = index->count; i > 0; i--)
	{
		v = get_offset(data, os);
		data += os;
		if (v < prev)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Index not monotonic");
		prev = v;
	}
	if (v > len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated index");

	data += prev - 1;
	index->index_size = data - data0;

	return index->index_size + offset;
}

static uint32_t
index_get(fz_context *ctx, index_t *index, int idx)
{
	int os;
	uint32_t v;

	if (idx < 0 || idx > index->count || index->count == 0)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Index bounds");

	os = index->offsize;
	idx *= os;
	v = get_offset(&index->offset[idx], index->offsize);

	return index->data_offset + v;
}

/* DICT handling structures and functions */

#define HIOP(A) (A+22)

typedef enum
{
	/* Top DICT Operators */
	DICT_OP_version = 0,
	DICT_OP_Notice = 1,
	DICT_OP_Copyright = HIOP(0),
	DICT_OP_FullName = 2,
	DICT_OP_FamilyName = 3,
	DICT_OP_Weight = 4,
	DICT_OP_isFixedPitch = HIOP(1),
	DICT_OP_ItalicAngle = HIOP(2),
	DICT_OP_UnderlinePosition = HIOP(3),
	DICT_OP_UnderlineThickness = HIOP(4),
	DICT_OP_PaintType = HIOP(5),
	DICT_OP_CharstringType = HIOP(6),
	DICT_OP_FontMatrix = HIOP(7),
	DICT_OP_UniqueID = 13,
	DICT_OP_FontBBox = 5,
	DICT_OP_StrokeWidth = HIOP(8),
	DICT_OP_XUID = 14,
	DICT_OP_charset = 15,
	DICT_OP_Encoding = 16,
	DICT_OP_CharStrings = 17,
	DICT_OP_Private = 18,
	DICT_OP_SyntheticBase = HIOP(20),
	DICT_OP_Postscript = HIOP(21),
	DICT_OP_BaseFontName = HIOP(22),
	DICT_OP_BaseFontBlend = HIOP(23),

	/* CIDFont Operators */
	DICT_OP_ROS = HIOP(30),
	DICT_OP_CIDFontVersion = HIOP(31),
	DICT_OP_CIDFontRevision = HIOP(32),
	DICT_OP_CIDFontType = HIOP(33),
	DICT_OP_CIDCount = HIOP(34),
	DICT_OP_UIDBase = HIOP(35),
	DICT_OP_FDArray = HIOP(36),
	DICT_OP_FDSelect = HIOP(37),
	DICT_OP_FontName = HIOP(38),

	/* Private DICT Operators */
	DICT_OP_BlueValues = 6,
	DICT_OP_OtherBlues = 7,
	DICT_OP_FamilyBlues = 8,
	DICT_OP_FamilyOtherBlues = 9,
	DICT_OP_BlueScale = HIOP(9),
	DICT_OP_BlueShift = HIOP(10),
	DICT_OP_BlueFuzz = HIOP(11),
	DICT_OP_StdHW = 10,
	DICT_OP_StdVW = 11,
	DICT_OP_StemSnapH = HIOP(12),
	DICT_OP_StemSnapV = HIOP(13),
	DICT_OP_ForceBold = HIOP(14),
	DICT_OP_LanguageGroup = HIOP(17),
	DICT_OP_ExpansionFactor = HIOP(18),
	DICT_OP_initialRandomSeed = HIOP(19),
	DICT_OP_Subrs = 19,
	DICT_OP_defaultWidthX = 20,
	DICT_OP_nominalWidthX = 21
} dict_operator;

typedef enum
{
	da_int = 0,
	da_real = 1,
	da_operator = 2
} dict_arg_type;

typedef struct {
	dict_arg_type type;
	union {
		uint32_t i;
		float f;
	} u;
} dict_arg;

#define DICT_MAX_ARGS 48

typedef struct {
	const uint8_t *base;
	size_t len;
	uint32_t offset;
	uint32_t end_offset;
	uint8_t *val;
	int eod;
	int num_args;
	dict_arg arg[DICT_MAX_ARGS+1];
} dict_iterator;

static uint8_t
dict_get_byte(fz_context *ctx, dict_iterator *di)
{
	uint8_t b;

	if (di->offset == di->end_offset)
		di->eod = 1;
	if (di->eod)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Overlong DICT data");
	b = di->base[di->offset++];

	return b;
}

static dict_arg
dict_get_arg(fz_context *ctx, dict_iterator *di)
{
	uint8_t b0, b1, b2, b3, b4;
	dict_arg d;

	b0 = dict_get_byte(ctx, di);
	if (b0 == 12)
	{
		b1 = dict_get_byte(ctx, di);
		d.type = da_operator;
		d.u.i = HIOP(b1);
		return d;
	}
	else if (b0 <= 21)
	{
		d.type = da_operator;
		d.u.i = b0;
		return d;
	}
	else if (b0 <= 27)
	{
malformed:
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed DICT");
	}
	else if (b0 == 28)
	{
		b1 = dict_get_byte(ctx, di);
		b2 = dict_get_byte(ctx, di);
		d.type = da_int;
		d.u.i = (b1<<8) | b2;
	}
	else if (b0 == 29)
	{
		b1 = dict_get_byte(ctx, di);
		b2 = dict_get_byte(ctx, di);
		b3 = dict_get_byte(ctx, di);
		b4 = dict_get_byte(ctx, di);
		d.type = da_int;
		d.u.i = (b1<<24) | (b2<<16)  | (b3<<8)  | b4;
	}
	else if (b0 == 30)
	{
		char cheap[32+5];
		unsigned int i;

		for (i = 0; i < sizeof(cheap)-5; )
		{
			static const char *dict = "0123456789.EE -f";
			uint8_t b = dict_get_byte(ctx, di);

			if ((b>>4) == 0xf)
				break;
			cheap[i++] = dict[b>>4];
			if ((b>>4) == 0xc)
				cheap[i++] = '-';

			b &= 15;
			if (b == 0xf)
				break;
			cheap[i++] = dict[b];
			if (b == 0xc)
				cheap[i++] = '-';
		}
		cheap[i++] = 0;
		d.type = da_real;
		d.u.f = fz_atof(cheap);
	}
	else if (b0 == 31)
	{
		goto malformed;
	}
	else if (b0 <= 246)
	{
		d.type = da_int;
		d.u.i = b0-139;
	}
	else if (b0 <= 250)
	{
		b1 = dict_get_byte(ctx, di);
		d.type = da_int;
		d.u.i = ((b0-247)<<8) + b1 + 108;
	}
	else if (b0 <= 254)
	{
		b1 = dict_get_byte(ctx, di);
		d.type = da_int;
		d.u.i = -((b0-251)<<8) - b1 - 108;
	}
	else
		goto malformed;

	return d;
}

static dict_operator
dict_next(fz_context *ctx, dict_iterator *di)
{
	int n;

	if (di->offset >= di->end_offset)
	{
		di->eod = 1;
		return 0;
	}

	n = 0;
	while (di->offset < di->end_offset)
	{
		di->arg[n] = dict_get_arg(ctx, di);
		if (di->arg[n].type == da_operator)
		{
			/* Sorted! Terminate loop. */
			break;
		}
		if (n == DICT_MAX_ARGS)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Too many operands");
		n++;
	}
	di->num_args = n;

	return (dict_operator)di->arg[n].u.i;
}

static dict_operator
dict_init(fz_context *ctx, dict_iterator *di, const uint8_t *base, size_t len, uint32_t offset, uint32_t end)
{
	di->base = base;
	di->len = len;
	di->offset = offset;
	di->end_offset = end;
	di->eod = (di->offset == di->end_offset);

	if (di->offset > len || end > len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Malformed DICT");

	return dict_next(ctx, di);
}

static int
dict_more(dict_iterator *di)
{
	return !di->eod;
}

static uint32_t
dict_arg_int(fz_context *ctx, dict_iterator *di, int idx)
{
	if (idx < 0 || idx >= di->num_args)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Missing dict arg");

	if (di->arg[idx].type != da_int)
		fz_throw(ctx, FZ_ERROR_FORMAT, "DICT arg not an int");

	return di->arg[idx].u.i;
}

static void
dict_write_arg(fz_context *ctx, fz_output *out, dict_arg d)
{
	int si;
	uint32_t i = d.u.i;

	if (d.type == da_operator)
	{
		if (i >= HIOP(0))
		{
			fz_write_byte(ctx, out, 12);
			i -= HIOP(0);
		}
		fz_write_byte(ctx, out, i);
		return;
	}

	if (d.type == da_real)
	{
		char text[32];
		unsigned int k, j;
		uint8_t v;

		fz_snprintf(text, sizeof(text)-1, "%g", d.u.f);

		fz_write_byte(ctx, out, 30);
		j = 4;
		v = 0;
		for (k = 0; k < sizeof(text)-1;)
		{
			char c = text[k++];

			if (c >= '0' && c <= '9')
				v |= (c - '0')<<j;
			else if (c == '.')
				v |= 0xa<<j;
			else if (c == 'e' || c == 'E')
			{
				if (text[k] == '-')
				{
					v |= 0xc<<j;
					k++;
				}
				else
				{
					v |= 0xb<<j;
				}
			}
			else if (c == '-')
			{
				v |= 0xe<<j;
			}
			else if (c == 0)
				break;

			if (j == 0)
			{
				fz_write_byte(ctx, out, v);
				v = 0;
			}
			j ^= 4;
		}
		if (j == 4)
			v = 0xff;
		else
			v |= 0xf;
		fz_write_byte(ctx, out, v);
		return;
	}

	/* Must be an int. */
	si = (int)i;
	if (-107 <= si && si <= 107)
		fz_write_byte(ctx, out, si+139);
	else if (108 <= si && si <= 1131)
	{
		si -= 108;
		fz_write_byte(ctx, out, (si>>8)+247);
		fz_write_byte(ctx, out, si);
	}
	else if (-1131 <= si && si <= -108)
	{
		si = -si - 108;
		fz_write_byte(ctx, out, (si>>8)+251);
		fz_write_byte(ctx, out, si);
	}
	else if (-32768 <= si && si <= 32767)
	{
		fz_write_byte(ctx, out, 28);
		fz_write_byte(ctx, out, si>>8);
		fz_write_byte(ctx, out, si);
	}
	else
	{
		fz_write_byte(ctx, out, 29);
		fz_write_byte(ctx, out, si>>24);
		fz_write_byte(ctx, out, si>>16);
		fz_write_byte(ctx, out, si>>8);
		fz_write_byte(ctx, out, si);
	}
}

static void
dict_write_args(fz_context *ctx, fz_output *out, dict_iterator *di)
{
	int i;

	for (i = 0; i <= di->num_args; i++)
	{
		dict_write_arg(ctx, out, di->arg[i]);
	}
}

static void
do_subset(fz_context *ctx, cff_t *cff, fz_buffer **buffer, usage_list_t *keep_list, index_t *index, int keep_notdef)
{
	uint8_t *d, *strings;
	uint32_t i, offset, end;
	uint32_t required, offset_size, fill;
	uint32_t num_charstrings = index->count;
	int gid;
	int num_gids = keep_list->len;
	const usage_t *gids = keep_list->list;

	if (num_charstrings == 0)
		return;

	/* First count the required size. */
	offset = index_get(ctx, index, 0);
	required = 0;
	gid = 0;
	for (i = 0; i < num_charstrings; offset = end, i++)
	{
		end = index_get(ctx, index, i+1);
		if (gid < num_gids && i == gids[gid].num)
		{
			/* Keep this */
			gid++;
		}
		else if (keep_notdef && i == 0)
		{
			/* Keep this. */
		}
		else
		{
			/* Drop this */
			required += 1;
			continue;
		}
		required += end-offset;
	}

	/* So we need 'required' bytes of space for the strings themselves */
	/* Do not forget to increment by one byte! This is because the
	last entry in the offset table points to one byte beyond the end of
	the required string data. Consider if the required string data occupies
	255 bytes, then each offset for each of the required entries can be
	represented by a single byte, but the last table entry would need to
	point to offset 256, which cannot be represented by a single byte. */
	offset_size = offsize_for_offset(required + 1);

	required += 2 + 1 + (num_charstrings+1)*offset_size;

	*buffer = fz_new_buffer(ctx, required);
	d = (*buffer)->data;
	(*buffer)->len = required;

	/* Write out the index header */
	put16(d, num_charstrings); /* count */
	d +=2;
	put8(d, offset_size); /* offset size */
	d += 1;


	/* Now copy the charstrings themselves */
	strings = d + offset_size * (num_charstrings+1) - 1;
	gid = 0;
	fill = 1;
	offset = index_get(ctx, index, 0);
	for (i = 0; i < num_charstrings; offset = end, i++)
	{
		end = index_get(ctx, index, i+1);
		if (gid < num_gids && gids[gid].num == i)
		{
			/* Keep this */
			gid++;
		}
		else if (keep_notdef && i == 0)
		{
			/* Keep this */
		}
		else
		{
			/* Drop this */
			put_offset(d, offset_size, fill);
			d += offset_size;
			strings[fill++] = 0x0e; /* endchar */
			continue;
		}

		memcpy(strings + fill, &cff->base[offset], end-offset);
		put_offset(d, offset_size, fill);
		d += offset_size;
		fill += end-offset;
	}
	put_offset(d, offset_size, fill);
}

static void
subset_charstrings(fz_context *ctx, cff_t *cff)
{
	do_subset(ctx, cff, &cff->charstrings_subset, &cff->gids_to_keep, &cff->charstrings_index, 1);
}

static void
subset_locals(fz_context *ctx, cff_t *cff)
{
	do_subset(ctx, cff, &cff->local_subset, &cff->local_usage, &cff->local_index, 0);
}

static void
subset_globals(fz_context *ctx, cff_t *cff)
{
	do_subset(ctx, cff, &cff->global_subset, &cff->global_usage, &cff->global_index, 0);
}

static void
subset_fdarray_locals(fz_context *ctx, cff_t *cff)
{
	uint16_t i, n = cff->fdarray_index.count;

	for (i = 0; i < n; i++)
		do_subset(ctx, cff, &cff->fdarray[i].local_subset, &cff->fdarray[i].local_usage, &cff->fdarray[i].local_index, 0);
}

/* Charstring "executing" functions */

static int
usage_list_find(fz_context *ctx, usage_list_t *list, int value)
{
	/* are we on the list already? */
	int lo = 0;
	int hi = list->len;

	while (lo < hi)
	{
		int mid = (lo + hi)>>1;
		int v = list->list[mid].num;
		if (v < value)
			lo = mid+1;
		else if (v > value)
			hi = mid;
		else
			return mid;
	}
	return lo;
}

static int
usage_list_contains(fz_context *ctx, usage_list_t *list, int value)
{
	int lo = usage_list_find(ctx, list, value);

	return (lo < list->len && list->list[lo].num == value);
}

static void
usage_list_add(fz_context *ctx, usage_list_t *list, int value)
{
	int lo = usage_list_find(ctx, list, value);

	if (lo < list->len && list->list[lo].num == value)
		return;

	if (list->len == list->max)
	{
		int newmax = list->max * 2;

		if (newmax == 0)
			newmax = 32;

		list->list = fz_realloc(ctx, list->list, sizeof(*list->list) * newmax);
		list->max = newmax;
	}

	memmove(&list->list[lo+1], &list->list[lo], (list->len - lo) * sizeof(*list->list));
	list->list[lo].num = value;
	list->list[lo].scanned = 0;
	list->len++;
}

static void
drop_usage_list(fz_context *ctx, usage_list_t *list)
{
	if (!list)
		return;
	fz_free(ctx, list->list);
	list->list = NULL;
}

static void
mark_subr_used(fz_context *ctx, cff_t *cff, int subr, int global, int local_subr_bias, usage_list_t *local_usage)
{
	usage_list_t *list = global ? &cff->global_usage : local_usage;

	subr += global ? cff->gsubr_bias : local_subr_bias;

	usage_list_add(ctx, list, subr);
}

static void
use_sub_char(fz_context *ctx, cff_t *cff, int code)
{
	/* code is a character code in 'standard encoding'. We
	 * need to map that to whatever glyph that would be in
	 * standard encoding, and mark that glyph as being used. */
	uint32_t i, gid;

	if (code < 0 || code > 255)
		return;
	i = standard_encoding[code];
	if (i == 0)
		return;

	for (gid = 0; gid < cff->unpacked_charset_len; gid++)
	{
		if (cff->unpacked_charset[gid] == i)
			break;
	}
	if (gid == cff->unpacked_charset_len)
	{
		fz_warn(ctx, "subsidiary char out of range");
		return;
	}

	if (usage_list_contains(ctx, &cff->gids_to_keep, gid))
		return;

	usage_list_add(ctx, &cff->extra_gids_to_keep, gid);
}

#define ATLEAST(n) if (sp < n) goto atleast_fail;
#define POP(n) if (sp < n) goto atleast_fail;
#define PUSH(n) \
do { if (sp + n > (int)(sizeof(stack)/sizeof(*stack))) fz_throw(ctx, FZ_ERROR_FORMAT, "Stack overflow"); sp += n; } while (0)

static void
execute_charstring(fz_context *ctx, cff_t *cff, const uint8_t *pc, const uint8_t *end, uint16_t subr_bias, usage_list_t *local_usage)
{
	double trans[32] = { 0 };
	double stack[513];
	int sp = 0;
	int stem_hints = 0;
	uint8_t c;

	/* 0 => starting, 1 => had hstem, 2 => anything else */
	int start = 0;

	while (pc < end)
	{
		c = *pc++;

		/* An operator other than one of the hint ones immediately
		 * disqualifies us from being in the hint extension state. */
		if (c < 32 && (c != 1 && c != 18 && c != 19 && c != 20))
			start = 2;

		switch (c)
		{
		case 0:
		case 2:
		case 9:
		case 13:
		case 17:
			fz_throw(ctx, FZ_ERROR_FORMAT, "Reserved charstring byte c=0x%x", c);
			break;

		/* Deal with all the hints together */
		case 18: /* hstemhm */
		case 1: /* hstem */
			start = 1;
		case 23: /* vstemhm */
		case 3: /* vstem */
			stem_hints += (sp/2);
			goto clear;

		case 19: /* hintmask */
		case 20: /* cntrmask */
			if (start == 1)
				stem_hints += (sp/2);
			pc += (stem_hints+7)>>3;
			if (pc > end)
				goto overflow;
			start = 2;
			goto clear;

		/* The operators all clear the stack. */
		case 4: /* vmoveto */
		case 5: /* rlineto */
		case 6: /* hlineto */
		case 7: /* vlineto */
		case 8: /* rrcurveto */
		case 15: /* vsindex */
		case 21: /* rmoveto */
		case 22: /* hmoveto */
		case 24: /* rcurveline */
		case 25: /* rlinecurve */
		case 26: /* vvcurveto */
		case 27: /* hhcurveto */
		case 30: /* vhcurveto */
		case 31: /* hvcurveto */
clear:
			sp = 0;
			break;



		case 10: /* callsubr */
			ATLEAST(1);
			mark_subr_used(ctx, cff, stack[sp-1], 0, subr_bias, local_usage);
			sp--;
			break;
		case 11: /* return */
			pc = end;
			sp = 0;
			break;
		case 12: /* escape */
		{
			if (pc == end)
			{
overflow:
				fz_throw(ctx, FZ_ERROR_FORMAT, "Buffer overflow in charstring");
			}
			c = *pc++;
			switch (c)
			{
			case 0: /* dotsection: deprecated, nop */
				sp = 0;
				break;
			case 3: /* and */
				ATLEAST(2);
				stack[sp-2] = (stack[sp-1] != 0 && stack[sp-2] != 0);
				sp--;
				break;
			case 4: /* or */
				ATLEAST(2);
				stack[sp-2] = (stack[sp-1] != 0 || stack[sp-2] != 0);
				sp--;
				break;
			case 5: /* not */
				ATLEAST(1);
				stack[sp-1] = (stack[sp-1] == 0);
				break;

			case 9: /* abs */
				ATLEAST(1);
				if (stack[sp-1] < 0)
					stack[sp-1] = -stack[sp-1];
				break;

			case 10: /* add */
				ATLEAST(2);
				stack[sp-2] += stack[sp-1];
				sp--;
				break;

			case 11: /* sub */
				ATLEAST(2);
				stack[sp-2] -= stack[sp-1];
				sp--;
				break;

			case 12: /* div */
				ATLEAST(2);
				if (stack[sp-2] != 0)
					stack[sp-2] /= stack[sp-1];
				sp--;
				break;

			case 14: /* neg */
				ATLEAST(1);
				stack[sp-1] = -stack[sp-1];
				break;

			case 15: /* eq */
				ATLEAST(2);
				stack[sp-2] = (stack[sp-1] == stack[sp-2]);
				sp--;
				break;
			case 18: /* drop */
				POP(1);
				break;

			case 20: /* put */
				ATLEAST(2);
				if ((int)stack[sp-1] < 0 || (unsigned int)stack[sp-1] > sizeof(trans)/sizeof(*trans))
					fz_throw(ctx, FZ_ERROR_FORMAT, "Transient array over/underflow");
				trans[(int)stack[sp-1]] = stack[sp-2];
				sp -= 2;
				break;
			case 21: /* get */
				ATLEAST(1);
				if ((int)stack[sp-1] < 0 || (unsigned int)stack[sp-1] > sizeof(trans)/sizeof(*trans))
					fz_throw(ctx, FZ_ERROR_FORMAT, "Transient array over/underflow");
				stack[sp-1] = trans[(int)stack[sp-1]];
				break;

			case 22: /* ifelse */
				ATLEAST(4);
				if (stack[sp-2] > stack[sp-1])
					stack[sp-4] = stack[sp-3];
				sp -= 3;
				break;
			case 23: /* random */
				PUSH(1);
				stack[sp-1] = 0.5;
				break;

			case 24: /* mul */
				ATLEAST(2);
				stack[sp-2] *= stack[sp-1];
				break;

			case 26: /* sqrt */
				ATLEAST(1);
				if (stack[sp-1] >= 0)
					stack[sp-1] = sqrtf(stack[sp-1]);
				break;

			case 27: /* dup */
				ATLEAST(1);
				PUSH(1);
				stack[sp-1] = stack[sp-2];
				break;

			case 28: /* exch */
			{
				double d;
				ATLEAST(2);
				d = stack[sp-1];
				stack[sp-1] = stack[sp-2];
				stack[sp-2] = d;
				break;
			}
			case 29: /* index */
			{
				int i;
				ATLEAST(1);
				i = (int)stack[sp-1];
				ATLEAST(i+1);
				if (i < 0 || i > sp-1)
					i = 0;
				stack[sp-1] = stack[sp-2-i];
				break;
			}
			case 30: /* roll */
			{
				int N, J;
				ATLEAST(2);
				J = stack[sp-1];
				N = stack[sp-2];
				if (N == 0)
					break;
				if (N < 0)
					fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid roll");
				ATLEAST(2+N);
				if (J < 0)
				{
					J = N - ((-J) % N);
					if (J == 0)
						break;
				}
				while (J--)
				{
					double t = stack[sp-2];
					int i;
					for (i = N-1; i > 0; i--)
					{
						stack[sp-2-i] = stack[sp-3-i];
					}
					stack[sp-2-N] = t;
				}
				break;
			}


			case 34: /* hflex */
			case 35: /* flex */
			case 36: /* hflex1 */
			case 37: /* flex1 */
				sp = 0;
				break;


			default:
				fz_throw(ctx, FZ_ERROR_FORMAT, "Reserved charstring byte c=0x%x", c);
			}
			break;
		}
		case 14: /* endchar */
			pc = end;
			if (sp >= 4)
			{
				use_sub_char(ctx, cff, stack[sp-1]);
				use_sub_char(ctx, cff, stack[sp-2]);
			}
			sp = 0;
			break;
		case 16: /* blend */
			/* Consumes a lot of operators, leaves n, where n = stack[sp-1]. */
			ATLEAST(1);
			sp = stack[sp-1];
			break;
		case 29: /* callgsubr */
			ATLEAST(1);
			mark_subr_used(ctx, cff, stack[sp-1], 1, subr_bias, local_usage);
			sp--;
			break;
		case 28: /* shortint */
			if (pc + 2 >= end)
			{
				pc = end;
				break;
			}
			PUSH(1);
			stack[sp-1] = (pc[0]<<8) | pc[1];
			pc += 2;
			break;
		case 255: /* number */
			if (pc + 4 >= end)
			{
				pc = end;
				break;
			}
			PUSH(1);
			stack[sp-1] = ((pc[0]<<24) | (pc[1]<<16) | (pc[2]<<8) | pc[3]) / 65536.0;
			pc += 4;
			break;
		case 247: case 248: case 249: case 250: /* number */
			PUSH(1);
			stack[sp-1] = (c-247) * 256 + 108;
			if (pc >= end)
				break;
			stack[sp-1] += *pc++;
			break;
		case 251: case 252: case 253: case 254: /* number */
			PUSH(1);
			stack[sp-1] = -((c-251) * 256 + 108);
			if (pc >= end)
				break;
			stack[sp-1] -= *pc++;
			break;
		default: /* 32-246 */
			PUSH(1);
			stack[sp-1] = c-139;
			break;
		}

	}
	return;
atleast_fail:
	fz_throw(ctx, FZ_ERROR_FORMAT, "Insufficient operators on the stack: op=%d", c);
}


usage_list_t *
get_font_locals(fz_context *ctx, cff_t *cff, int gid, int is_pdf_cidfont, uint16_t *subr_bias)
{
	usage_t *gids = cff->gids_to_keep.list;
	int num_gids = cff->gids_to_keep.len;

	if (is_pdf_cidfont && cff->is_cidfont)
	{
		uint8_t font = 0;
		if (gid < num_gids && gids[gid].num < cff->charstrings_index.count)
			font = cff->gid_to_font[gids[gid].num];
		else if (gid == 0)
			font = cff->gid_to_font[gid];
		if (font >= cff->fdarray_index.count)
			font = 0;

		if (subr_bias)
			*subr_bias = cff->fdarray[font].subr_bias;
		return &cff->fdarray[font].local_usage;
	}

	if (subr_bias)
		*subr_bias = cff->subr_bias;
	return &cff->local_usage;
}

static void
scan_charstrings(fz_context *ctx, cff_t *cff, int is_pdf_cidfont)
{
	uint32_t offset, end;
	int num_charstrings = (int)cff->charstrings_index.count;
	int i, gid, font;
	usage_t *gids = cff->gids_to_keep.list;
	int num_gids = cff->gids_to_keep.len;
	int changed;
	uint16_t subr_bias;
	usage_list_t *local_usage = NULL;

	/* Scan through the charstrings.*/
	offset = index_get(ctx, &cff->charstrings_index, 0);
	gid = 0;
	for (i = 0; i < num_charstrings; offset = end, i++)
	{
		end = index_get(ctx, &cff->charstrings_index, i+1);
		if (gid < num_gids && i == gids[gid].num)
		{
			/* Keep this */
			gid++;
		}
		else if (i == 0)
		{
			/* Keep this. */
		}
		else
		{
			/* Drop this */
			continue;
		}
		local_usage = get_font_locals(ctx, cff, gid, is_pdf_cidfont, &subr_bias);
		execute_charstring(ctx, cff, &cff->base[offset], &cff->base[end], subr_bias, local_usage);
	}

	/* Now we search the 'extra' ones, the 'subrs' (local) and 'gsubrs' (globals)
	 * that are used. Searching each of these might find more that need to be
	 * searched, so we use a loop. */
	do
	{
		changed = 0;
		/* Extra (subsidiary) glyphs */
		for (i = 0; i < cff->extra_gids_to_keep.len; i++)
		{
			if (cff->extra_gids_to_keep.list[i].scanned)
				continue;
			cff->extra_gids_to_keep.list[i].scanned = 1;
			gid = cff->extra_gids_to_keep.list[i].num;
			usage_list_add(ctx, &cff->gids_to_keep, gid);
			offset = index_get(ctx, &cff->charstrings_index, gid);
			end = index_get(ctx, &cff->charstrings_index, gid+1);

			local_usage = get_font_locals(ctx, cff, gid, is_pdf_cidfont, &subr_bias);
			execute_charstring(ctx, cff, &cff->base[offset], &cff->base[end], subr_bias, local_usage);
			changed = 1;
		}

		/* Now, run through the locals, seeing what locals and globals they call.  */
		for (i = 0; i < cff->local_usage.len; i++)
		{
			if (cff->local_usage.list[i].scanned)
				continue;
			cff->local_usage.list[i].scanned = 1;
			gid = cff->local_usage.list[i].num;
			offset = index_get(ctx, &cff->local_index, gid);
			end = index_get(ctx, &cff->local_index, gid+1);

			local_usage = get_font_locals(ctx, cff, gid, is_pdf_cidfont, &subr_bias);
			execute_charstring(ctx, cff, &cff->base[offset], &cff->base[end], subr_bias, local_usage);
			changed = 1;
		}

		/* Now, run through the per-font locals, seeing what per-font locals and globals they call.  */
		for (font = 0; font < cff->fdarray_index.count; font++)
		{
			for (i = 0; i < cff->fdarray[font].local_usage.len; i++)
			{
				gid = cff->fdarray[font].local_usage.list[i].num;

				if (cff->fdarray[font].local_usage.list[i].scanned)
					continue;
				cff->fdarray[font].local_usage.list[i].scanned = 1;
				gid = cff->fdarray[font].local_usage.list[i].num;
				offset = index_get(ctx, &cff->fdarray[font].local_index, gid);
				end = index_get(ctx, &cff->fdarray[font].local_index, gid+1);

				local_usage = get_font_locals(ctx, cff, gid, is_pdf_cidfont, &subr_bias);
				execute_charstring(ctx, cff, &cff->base[offset], &cff->base[end], subr_bias, local_usage);
				changed = 1;
			}
		}

		/* Now, run through the globals, seeing what globals they call.  */
		for (i = 0; i < cff->global_usage.len; i++)
		{
			if (cff->global_usage.list[i].scanned)
				continue;
			cff->global_usage.list[i].scanned = 1;
			gid = cff->global_usage.list[i].num;
			offset = index_get(ctx, &cff->global_index, gid);
			end = index_get(ctx, &cff->global_index, gid+1);

			local_usage = get_font_locals(ctx, cff, gid, is_pdf_cidfont, &subr_bias);
			execute_charstring(ctx, cff, &cff->base[offset], &cff->base[end], subr_bias, local_usage);
			changed = 1;
		}
	}
	while (changed);
}

static void
get_encoding_len(fz_context *ctx, cff_t *cff)
{
	uint32_t encoding_offset = cff->encoding_offset;
	const uint8_t *d = cff->base + encoding_offset;
	uint8_t fmt;
	uint8_t n;
	uint32_t size;

	if (encoding_offset < 2)
	{
		cff->encoding_len = 0;
		return;
	}

	if (encoding_offset + 2 > cff->len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt encoding");

	fmt = *d++;
	n = *d++;

	switch (fmt & 127)
	{
	case 0:
		size = 2 + n;
		break;
	case 1:
		size = 2 + n * 2;
		break;
	case 2:
		size = 2 + n * 3;
		break;
	default:
		fz_throw(ctx, FZ_ERROR_FORMAT, "Bad format encoding");
	}

	if (encoding_offset + size > cff->len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt encoding");

	if (fmt & 128)
	{
		if (encoding_offset + size + 1 > cff->len)
			fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt encoding");

		n = *d++;
		size += 1 + n*3;

		if (encoding_offset + size > cff->len)
			fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt encoding");
	}
	cff->encoding_len = size;
}

static void
get_charset_len(fz_context *ctx, cff_t *cff)
{
	uint32_t charset_offset = cff->charset_offset;
	const uint8_t *d = cff->base + charset_offset;
	const uint8_t *d0 = d;
	uint8_t fmt;
	uint32_t i, n;

	if (charset_offset < 2)
	{
		cff->charset_len = 0;
		return;
	}

	if (charset_offset + 1 > cff->len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt charset");

	fmt = *d++;
	n = cff->charstrings_index.count;

	if (fmt == 0)
	{
		cff->unpacked_charset = fz_malloc(ctx, sizeof(uint16_t) * n);
		cff->unpacked_charset_len = cff->unpacked_charset_max = n;
		cff->unpacked_charset[0] = 0;
		for (i = 1; i < n; i++)
		{
			cff->unpacked_charset[i] = get16(d);
			d += 2;
		}
	}
	else if (fmt == 1)
	{
		cff->unpacked_charset = fz_malloc(ctx, sizeof(uint16_t) * 256);
		cff->unpacked_charset_max = 256;
		cff->unpacked_charset_len = 1;
		cff->unpacked_charset[0] = 0;
		n--;
		while (n > 0)
		{
			uint16_t first;
			uint32_t nleft;
			if (d + 3>= cff->base + cff->len)
				fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt charset");
			first = get16(d);
			nleft = d[2] + 1;
			d += 3;
			if (nleft > n)
				fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt charset");
			n -= nleft;
			while (nleft)
			{
				if (cff->unpacked_charset_len == cff->unpacked_charset_max)
				{
					cff->unpacked_charset = fz_realloc(ctx, cff->unpacked_charset, sizeof(uint16_t) * 2 * cff->unpacked_charset_max);
					cff->unpacked_charset_max *= 2;
				}
				cff->unpacked_charset[cff->unpacked_charset_len++] = first;
				first++;
				nleft--;
			}
		}
	}
	else if (fmt == 2)
	{
		cff->unpacked_charset = fz_malloc(ctx, sizeof(uint16_t) * 256);
		cff->unpacked_charset_max = 256;
		cff->unpacked_charset_len = 1;
		cff->unpacked_charset[0] = 0;
		n--;
		while (n > 0)
		{
			uint16_t first;
			uint32_t nleft;
			if (d + 4 >= cff->base + cff->len)
				fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt charset");
			first = get16(d);
			nleft = get16(d+2) + 1;
			d += 4;
			if (nleft > n)
				fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt charset");
			n -= nleft;
			while (nleft)
			{
				if (cff->unpacked_charset_len == cff->unpacked_charset_max)
				{
					cff->unpacked_charset = fz_realloc(ctx, cff->unpacked_charset, sizeof(uint16_t) * 2 * cff->unpacked_charset_max);
					cff->unpacked_charset_max *= 2;
				}
				cff->unpacked_charset[cff->unpacked_charset_len++] = first;
				first++;
				nleft--;
			}
		}
	}
	else
	{
		fz_throw(ctx, FZ_ERROR_FORMAT, "Bad charset format");
	}

	cff->charset_len = (uint32_t)(d - d0);
}

static void
read_fdselect(fz_context *ctx, cff_t *cff)
{
	uint32_t fdselect_offset = cff->fdselect_offset;
	const uint8_t *d = cff->base + fdselect_offset;
	const uint8_t *d0 = d;
	uint8_t fmt;
	uint16_t n, m, i, first, last, k;

	if (fdselect_offset == 0)
	{
		cff->fdselect_len = 0;
		return;
	}

	if (fdselect_offset + 1 > cff->len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt fdselect");

	fmt = *d++;
	n = cff->charstrings_index.count;

	cff->gid_to_font = fz_calloc(ctx, n, sizeof(*cff->gid_to_font));

	if (fmt == 0)
	{
		for (i = 0; i < n; i++)
		{
			if (d >= cff->base + cff->len)
				fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt fdselect");
			cff->gid_to_font[i] = d[0];
			d++;
		}
	}
	else if (fmt == 3)
	{
		if (d + 2 >= cff->base + cff->len)
			fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt fdselect");
		m = get16(d);
		d += 2;
		if (m > cff->charstrings_index.count)
			fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt fdselect");

		for (i = 0; i < m; i++)
		{
			if (d + 5 >= cff->base + cff->len)
				fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt fdselect");
			first = get16(d);
			last = get16(d + 3);
			if (first >= cff->charstrings_index.count || last > cff->charstrings_index.count || first >= last)
				fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt fdselect");
			for (k = first; k < last; k++)
				cff->gid_to_font[k] = d[2];
			d += 3;
		}
	}

	cff->fdselect_len = (uint32_t)(d - d0);
}

static void
load_charset_for_cidfont(fz_context *ctx, cff_t *cff)
{
	uint32_t charset_offset = cff->charset_offset;
	const uint8_t *d = cff->base + charset_offset;
	uint8_t fmt;
	uint32_t n = cff->charstrings_index.count;
	uint32_t i;

	if (charset_offset + 1 > cff->len)
		fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt charset");

	fmt = *d++;

	cff->gid_to_cid = fz_calloc(ctx, n, sizeof(*cff->gid_to_cid));
	cff->gid_to_cid[0] = 0;

	if (fmt == 0)
	{
		for (i = 1; i < n; i++)
		{
			cff->gid_to_cid[i] = get16(d);
			d += 2;
		}
	}
	else if (fmt == 1)
	{
		for (i = 1; i < n;)
		{
			uint16_t first;
			int32_t nleft;
			if (d + 3 >= cff->base + cff->len)
				fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt charset");
			first = get16(d);
			nleft = d[2] + 1;
			d += 3;
			while (nleft-- && i < n)
			{
				cff->gid_to_cid[i++] = first++;
			}
		}
	}
	else if (fmt == 2)
	{
		for (i = 1; i < n;)
		{
			uint16_t first;
			int32_t nleft;
			if (d + 4 >= cff->base + cff->len)
				fz_throw(ctx, FZ_ERROR_FORMAT, "corrupt charset");
			first = get16(d);
			nleft = get16(d+2) + 1;
			d += 4;
			while (nleft-- && i < n)
			{
				cff->gid_to_cid[i++] = first++;
			}
		}
	}
	else
	{
		fz_throw(ctx, FZ_ERROR_FORMAT, "Bad charset format");
	}
}

static void
write_offset(fz_context *ctx, fz_output *out, uint8_t os, uint32_t v)
{
	if (os > 3)
		fz_write_byte(ctx, out, v>>24);
	if (os > 2)
		fz_write_byte(ctx, out, v>>16);
	if (os > 1)
		fz_write_byte(ctx, out, v>>8);
	fz_write_byte(ctx, out, v);
}

static void
output_name_index(fz_context *ctx, cff_t *cff, fz_output *out)
{
	uint32_t name0 = index_get(ctx, &cff->name_index, 0);
	uint32_t name1 = index_get(ctx, &cff->name_index, 1);
	uint8_t os;

	/* Turn name1 back into an offset from the index. */
	name1 -= name0;
	name1++;
	os = offsize_for_offset(name1);

	fz_write_uint16_be(ctx, out, 1); /* Count */
	fz_write_byte(ctx, out, os); /* offsize */
	write_offset(ctx, out, os, 1); /* index[0] = 1 */
	write_offset(ctx, out, os, name1); /* index[1] = end */
	fz_write_data(ctx, out, cff->base + name0, name1-1);
}

static void
output_top_dict_index(fz_context *ctx, cff_t *cff, fz_output *out)
{
	uint32_t top_dict_len = (uint32_t)cff->top_dict_subset->len;
	uint8_t os = offsize_for_offset((uint32_t)(1 + top_dict_len));

	fz_write_uint16_be(ctx, out, 1); /* Count */
	fz_write_byte(ctx, out, os); /* offsize */
	write_offset(ctx, out, os, 1);
	write_offset(ctx, out, os, (uint32_t)(1 + cff->top_dict_subset->len));

	/* And copy the updated top dict. */
	fz_write_data(ctx, out, cff->top_dict_subset->data, cff->top_dict_subset->len);
}

static uint32_t
rewrite_fdarray(fz_context *ctx, cff_t *cff, uint32_t offset0)
{
	/* fdarray_index will start at offset0. */
	uint16_t i;
	uint16_t n = cff->fdarray_index.count;
	uint32_t len = 0;
	uint8_t os;
	size_t offset;

	if (cff->fdarray == NULL)
		fz_throw(ctx, FZ_ERROR_FORMAT, "Expected to rewrite an fdarray");

	/* Count how many bytes the index will require. */
	for (i = 0; i < n; i++)
	{
		len += (uint32_t)cff->fdarray[i].rewritten_dict->len;
	}
	os = offsize_for_offset(len+1);
	len += 2 + 1 + (n+1)*os;

	/* Now offset0 + len points to where the private dicts
	 * will go. Run through, fixing up the offsets in the
	 * font dicts (this won't change the length). */
	offset = offset0 + len;
	for (i = 0; i < n; i++)
	{
		assert(cff->fdarray[i].rewritten_dict->data[cff->fdarray[i].fixup] == 29);
		assert(cff->fdarray[i].rewritten_dict->data[cff->fdarray[i].fixup+5] == 29);
		put32(&cff->fdarray[i].rewritten_dict->data[cff->fdarray[i].fixup+1], (uint32_t)cff->fdarray[i].rewritten_private->len);
		put32(&cff->fdarray[i].rewritten_dict->data[cff->fdarray[i].fixup+6], (uint32_t)offset);
		offset += cff->fdarray[i].rewritten_private->len;
		if (cff->fdarray[i].local_subset)
		{
			offset += cff->fdarray[i].local_subset->len;
		}
		else
		{
			offset += 2;
		}
	}

	return (uint32_t)offset;
}

static void
update_dicts(fz_context *ctx, cff_t *cff, uint32_t offset)
{
	uint8_t *top_dict_data = cff->top_dict_subset->data;
	uint32_t top_dict_len = (uint32_t)cff->top_dict_subset->len;

	/* Update the offsets */
	/*	Header
		Name Index
		Top Dict Index
			(Top Dict)
		String Index
		Global Subr Index
		Encodings
		Charsets
		FDSelect
		CharStrings Index
		Font DICT Index
			(Font Dict)
		Private DICT
		Local Subr Index
	*/
	offset += 2 + 1 + 2 * offsize_for_offset(top_dict_len+1); /* offset = start of top_dict_index data */
	offset += top_dict_len; /* offset = end of top_dict */
	if (cff->string_index.index_size)
		offset += cff->string_index.index_size;
	else
		offset += 2;
	if (cff->global_subset)
		offset += (uint32_t)cff->global_subset->len;
	else if (cff->global_index.index_size)
		offset += cff->global_index.index_size;
	else
		offset += 2;
	if (cff->top_dict_fixup_offsets.encoding)
	{
		assert(top_dict_data[cff->top_dict_fixup_offsets.encoding] == 29);
		put32(top_dict_data + cff->top_dict_fixup_offsets.encoding+1, offset);
		offset += cff->encoding_len;
	}
	if (cff->top_dict_fixup_offsets.charset)
	{
		assert(top_dict_data[cff->top_dict_fixup_offsets.charset] == 29);
		put32(top_dict_data + cff->top_dict_fixup_offsets.charset+1, offset);
		offset += cff->charset_len;
	}
	if (cff->top_dict_fixup_offsets.fdselect)
	{
		assert(top_dict_data[cff->top_dict_fixup_offsets.fdselect] == 29);
		put32(top_dict_data + cff->top_dict_fixup_offsets.fdselect+1, offset);
		offset += cff->fdselect_len;
	}
	assert(top_dict_data[cff->top_dict_fixup_offsets.charstrings] == 29);
	put32(top_dict_data + cff->top_dict_fixup_offsets.charstrings+1, offset);
	if (cff->charstrings_subset)
		offset += (uint32_t)cff->charstrings_subset->len;
	else if (cff->charstrings_index.index_size)
		offset += cff->charstrings_index.index_size;
	else
		offset += 2;
	if (cff->top_dict_fixup_offsets.fdarray)
	{
		assert(top_dict_data[cff->top_dict_fixup_offsets.fdarray] == 29);
		put32(top_dict_data + cff->top_dict_fixup_offsets.fdarray+1, offset);
		offset = rewrite_fdarray(ctx, cff, offset);
	}
	if (cff->top_dict_fixup_offsets.privat)
	{
		assert(top_dict_data[cff->top_dict_fixup_offsets.privat] == 29);
		put32(top_dict_data + cff->top_dict_fixup_offsets.privat+1, (uint32_t)cff->private_subset->len);
		put32(top_dict_data + cff->top_dict_fixup_offsets.privat+6, offset);
	}
}

static void
read_top_dict(fz_context *ctx, cff_t *cff, int idx)
{
	dict_iterator di;
	dict_operator k;
	uint32_t top_dict_offset = index_get(ctx, &cff->top_dict_index, idx);
	uint32_t top_dict_end = index_get(ctx, &cff->top_dict_index, idx+1);

	for (k = dict_init(ctx, &di, cff->base, cff->len, top_dict_offset, top_dict_end); dict_more(&di); k = dict_next(ctx, &di))
	{
		switch (k)
		{
		case DICT_OP_ROS:
			cff->is_cidfont = 1;
			break;
		case DICT_OP_charset:
			cff->charset_offset = dict_arg_int(ctx, &di, 0);
			break;
		case DICT_OP_Encoding:
			cff->encoding_offset = dict_arg_int(ctx, &di, 0);
			break;
		case DICT_OP_CharstringType:
			cff->charstring_type = 1;
			break;
		case DICT_OP_CharStrings:
			cff->charstrings_index_offset = dict_arg_int(ctx, &di, 0);
			break;
		case DICT_OP_Private:
			cff->private_len = dict_arg_int(ctx, &di, 0);
			cff->private_offset = dict_arg_int(ctx, &di, 1);
			break;
		case DICT_OP_FDSelect:
			cff->fdselect_offset = dict_arg_int(ctx, &di, 0);
			break;
		case DICT_OP_FDArray:
			cff->fdarray_index_offset = dict_arg_int(ctx, &di, 0);
			break;
		default:
			break;
		}
	}

	for (k = dict_init(ctx, &di, cff->base, cff->len, cff->private_offset, cff->private_offset + cff->private_len); dict_more(&di); k = dict_next(ctx, &di))
	{
		switch (k)
		{
		case DICT_OP_Subrs:
			cff->local_index_offset = dict_arg_int(ctx, &di, 0) + cff->private_offset;
			break;
		default:
			break;
		}
	}
}

static void
make_new_top_dict(fz_context *ctx, cff_t *cff)
{
	dict_iterator di;
	dict_operator k;
	uint32_t top_dict_offset = index_get(ctx, &cff->top_dict_index, 0);
	uint32_t top_dict_end = index_get(ctx, &cff->top_dict_index, 1);
	fz_output *out = NULL;

	cff->top_dict_subset = fz_new_buffer(ctx, 1024);

	fz_var(out);

	fz_try(ctx)
	{
		out = fz_new_output_with_buffer(ctx, cff->top_dict_subset);

		for (k = dict_init(ctx, &di, cff->base, cff->len, top_dict_offset, top_dict_end); dict_more(&di); k = dict_next(ctx, &di))
		{
			switch (k)
			{
			case DICT_OP_charset:
				if (cff->charset_offset < 2)
					di.arg[0].u.i = cff->charset_offset;
				else
				{
					di.arg[0].u.i = 0x80000000;
					cff->top_dict_fixup_offsets.charset = fz_tell_output(ctx, out);
				}
				break;
			case DICT_OP_Encoding:
				if (cff->encoding_offset < 2)
					di.arg[0].u.i = cff->encoding_offset;
				else
				{
					di.arg[0].u.i = 0x80000000;
					cff->top_dict_fixup_offsets.encoding = fz_tell_output(ctx, out);
				}
				break;
			case DICT_OP_CharStrings:
				di.arg[0].u.i = 0x80000000;
				cff->top_dict_fixup_offsets.charstrings = fz_tell_output(ctx, out);
				break;
			case DICT_OP_Private:
				di.arg[0].u.i = 0x80000000;
				di.arg[1].u.i = 0x80000000;
				cff->top_dict_fixup_offsets.privat = fz_tell_output(ctx, out);
				break;
			case DICT_OP_FDSelect:
				di.arg[0].u.i = 0x80000000;
				cff->top_dict_fixup_offsets.fdselect = fz_tell_output(ctx, out);
				break;
			case DICT_OP_FDArray:
				di.arg[0].u.i = 0x80000000;
				cff->top_dict_fixup_offsets.fdarray = fz_tell_output(ctx, out);
				break;
			default:
				break;
			}
			dict_write_args(ctx, out, &di);
		}

		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
make_new_private_dict(fz_context *ctx, cff_t *cff)
{
	dict_iterator di;
	dict_operator k;
	fz_output *out = NULL;
	int64_t len;

	cff->private_subset = fz_new_buffer(ctx, 1024);

	fz_var(out);

	fz_try(ctx)
	{
		int subrs = 0;
		out = fz_new_output_with_buffer(ctx, cff->private_subset);

		for (k = dict_init(ctx, &di, cff->base, cff->len, cff->private_offset, cff->private_offset + cff->private_len); dict_more(&di); k = dict_next(ctx, &di))
		{
			switch (k)
			{
			case DICT_OP_Subrs:
				subrs = 1;
				break;
			default:
				dict_write_args(ctx, out, &di);
			}
		}

		if (subrs != 0)
		{
			/* Everything is in the DICT except for the local subr offset. Insert
			 * that now. This is tricky, because what is the offset? It depends on
			 * the size of the dict we are creating now, and the size of the dict
			 * we are creating now depends on the size of the offset! */
			/* Length so far */
			len = fz_tell_output(ctx, out);
			/* We have to encode an offset, plus the Subrs token (19). Offset
			 * can take up to 5 bytes. */
			if (len+2 < 107)
			{
				/* We can code it with a single byte encoding */
				len += 2;
				fz_write_byte(ctx, out, len + 139);
			}
			else if (len+3 < 1131)
			{
				/* We can code it with a 2 byte encoding */
				/* (b0-247) * 256 + b1 + 108 == len+3 */
				len = len+3 - 108;
				fz_write_byte(ctx, out, (len>>8) + 247);
				fz_write_byte(ctx, out, len);
			}
			else if (len+4 < 32767)
			{
				/* We can code it with a 3 byte encoding */
				len += 4;
				fz_write_byte(ctx, out, 28);
				fz_write_byte(ctx, out, len>>8);
				fz_write_byte(ctx, out, len);
			}
			else
			{
				/* We can code it with a 5 byte encoding */
				len += 5;
				fz_write_byte(ctx, out, 29);
				fz_write_byte(ctx, out, len>>24);
				fz_write_byte(ctx, out, len>>16);
				fz_write_byte(ctx, out, len>>8);
				fz_write_byte(ctx, out, len);
			}
			fz_write_byte(ctx, out, DICT_OP_Subrs);
		}

		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
read_fdarray_and_privates(fz_context *ctx, cff_t *cff)
{
	dict_iterator di;
	dict_operator k;
	uint16_t i;
	uint16_t n = cff->fdarray_index.count;
	int subrs;
	int64_t len;

	cff->fdarray = fz_calloc(ctx, n, sizeof(*cff->fdarray));

	for (i = 0; i < n; i++)
	{
		uint32_t offset = index_get(ctx, &cff->fdarray_index, i);
		uint32_t end = index_get(ctx, &cff->fdarray_index, i+1);
		fz_output *out = NULL;

		cff->fdarray[i].rewritten_dict = fz_new_buffer(ctx, 1024);

		fz_var(out);

		fz_try(ctx)
		{
			out = fz_new_output_with_buffer(ctx, cff->fdarray[i].rewritten_dict);

			for (k = dict_init(ctx, &di, cff->base, cff->len, offset, end); dict_more(&di); k = dict_next(ctx, &di))
			{
				switch (k)
				{
				case DICT_OP_Private:
					cff->fdarray[i].len = di.arg[0].u.i;
					cff->fdarray[i].offset = di.arg[1].u.i;
					di.arg[0].u.i = 0x80000000;
					di.arg[1].u.i = 0x80000000;
					cff->fdarray[i].fixup = fz_tell_output(ctx, out);
					break;
				default:
					break;
				}
				dict_write_args(ctx, out, &di);
			}

			fz_close_output(ctx, out);
		}
		fz_always(ctx)
			fz_drop_output(ctx, out);
		fz_catch(ctx)
			fz_rethrow(ctx);


		offset = cff->fdarray[i].offset;
		end = cff->fdarray[i].offset + cff->fdarray[i].len;

		fz_try(ctx)
		{
			cff->fdarray[i].rewritten_private = fz_new_buffer(ctx, 1024);

			out = fz_new_output_with_buffer(ctx, cff->fdarray[i].rewritten_private);
			cff->fdarray[i].local_index_offset = 0;

			subrs = 0;

			for (k = dict_init(ctx, &di, cff->base, cff->len, offset, end); dict_more(&di); k = dict_next(ctx, &di))
			{
				switch (k)
				{
				case DICT_OP_Subrs:
					subrs = 1;
					cff->fdarray[i].local_index_offset = dict_arg_int(ctx, &di, 0) + offset;
					break;
				default:
					dict_write_args(ctx, out, &di);
					break;
				}
			}

			if (subrs != 0)
			{
				/* Everything is in the DICT except for the local subr offset. Insert
				 * that now. This is tricky, because what is the offset? It depends on
				 * the size of he dict we are creating now, and the size of the dict
				 * we are creating now depends on the size of the offset! */
				/* Length so far */
				len = fz_tell_output(ctx, out);
				/* We have to encode an offset, plus the Subrs token (19). Offset
				 * can take up to 5 bytes. */
				if (len+2 < 107)
				{
					/* We can code it with a single byte encoding */
					len += 2;
					fz_write_byte(ctx, out, len + 139);
				}
				else if (len+3 < 1131)
				{
					/* We can code it with a 2 byte encoding */
					/* (b0-247) * 256 + b1 + 108 == len+3 */
					len = len+3 - 108;
					fz_write_byte(ctx, out, (len>>8) + 247);
					fz_write_byte(ctx, out, len);
				}
				else if (len+4 < 32767)
				{
					/* We can code it with a 3 byte encoding */
					len += 4;
					fz_write_byte(ctx, out, 28);
					fz_write_byte(ctx, out, len>>8);
					fz_write_byte(ctx, out, len);
				}
				else
				{
					/* We can code it with a 5 byte encoding */
					len += 5;
					fz_write_byte(ctx, out, 29);
					fz_write_byte(ctx, out, len>>24);
					fz_write_byte(ctx, out, len>>16);
					fz_write_byte(ctx, out, len>>8);
					fz_write_byte(ctx, out, len);
				}
				fz_write_byte(ctx, out, DICT_OP_Subrs);
			}

			fz_close_output(ctx, out);
		}
		fz_always(ctx)
			fz_drop_output(ctx, out);
		fz_catch(ctx)
			fz_rethrow(ctx);

		if (cff->fdarray[i].local_index_offset != 0)
		{
			index_load(ctx, &cff->fdarray[i].local_index, cff->base, (uint32_t)cff->len, cff->fdarray[i].local_index_offset);
			cff->fdarray[i].subr_bias = subr_bias(ctx, cff, cff->fdarray[i].local_index.count);
		}
	}
}

static void
output_fdarray(fz_context *ctx, fz_output *out, cff_t *cff)
{
	uint16_t i;
	uint16_t n = cff->fdarray_index.count;
	uint8_t os;
	uint32_t offset = 1;
	uint32_t len = 0;

	for (i = 0; i < n; i++)
	{
		len += (uint32_t)cff->fdarray[i].rewritten_dict->len;
	}
	os = offsize_for_offset(len+1);

	fz_write_uint16_be(ctx, out, cff->fdarray_index.count); /* Count */
	fz_write_byte(ctx, out, os); /* offsize */

	/* First we write out the offsets of the rewritten dicts. */
	for (i = 0; i < n; i++)
	{
		write_offset(ctx, out, os, offset);
		offset += (uint32_t)cff->fdarray[i].rewritten_dict->len;
	}
	write_offset(ctx, out, os, offset);

	/* Now write the dicts themselves. */
	for (i = 0; i < n; i++)
	{
		fz_write_data(ctx, out, cff->fdarray[i].rewritten_dict->data, cff->fdarray[i].rewritten_dict->len);
	}

	/* Now we can write out the private dicts, unchanged from the original file. */
	for (i = 0; i < n; i++)
	{
		fz_write_data(ctx, out, cff->fdarray[i].rewritten_private->data, cff->fdarray[i].rewritten_private->len);
		if (cff->fdarray[i].local_subset)
			fz_write_data(ctx, out, cff->fdarray[i].local_subset->data, cff->fdarray[i].local_subset->len);
		else
			fz_write_uint16_be(ctx, out, 0);
	}
}

/* Nasty O(n^2) thing. */
static uint16_t
cid_to_gid(fz_context *ctx, cff_t *cff, uint16_t cid)
{
	uint32_t n = cff->charstrings_index.count;
	uint32_t i;

	for (i = 0; i < n; i++)
	{
		if (cff->gid_to_cid[i] == cid)
			return i;
	}
	return 0;
}


fz_buffer *
fz_subset_cff_for_gids(fz_context *ctx, fz_buffer *orig, int *gids, int num_gids, int symbolic, int is_pdf_cidfont)
{
	cff_t cff = { 0 };
	fz_buffer *newbuf = NULL;
	uint8_t *base;
	size_t len;
	fz_output *out = NULL;
	int i;
	uint16_t n, k;

	fz_var(newbuf);
	fz_var(out);

	if (orig == NULL)
		return NULL;

	base = orig->data;
	len = orig->len;

	fz_try(ctx)
	{
		cff.base = base;
		cff.len = len;

		cff.symbolic = symbolic;

		if (len < 4)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Truncated CFF");

		cff.major = base[0];
		cff.minor = base[1];
		cff.headersize = base[2];
		cff.offsize = base[3];

		if (cff.offsize > 4)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Invalid offsize in CFF");

		if (len > UINT32_MAX)
			fz_throw(ctx, FZ_ERROR_FORMAT, "CFF too large");

		/* First, the name index */
		cff.top_dict_index_offset = index_load(ctx, &cff.name_index, base, (uint32_t)len, cff.headersize);

		/* Next, the top dict index */
		cff.string_index_offset = index_load(ctx, &cff.top_dict_index, base, (uint32_t)len, cff.top_dict_index_offset);

		/* Next, the string index */
		cff.global_index_offset = index_load(ctx, &cff.string_index, base, (uint32_t)len, cff.string_index_offset);

		/* Next the Global subr index */
		index_load(ctx, &cff.global_index, base, (uint32_t)len, cff.global_index_offset);

		/* Default value, possibly updated by top dict entries */
		cff.charstring_type = 2;

		/* CFF files can contain several fonts, but we only want the first one. */
		read_top_dict(ctx, &cff, 0);

		cff.gsubr_bias = subr_bias(ctx, &cff, cff.global_index.count);

		if (cff.charstrings_index_offset == 0)
			fz_throw(ctx, FZ_ERROR_FORMAT, "Missing charstrings table");

		index_load(ctx, &cff.charstrings_index, base, (uint32_t)len, cff.charstrings_index_offset);
		index_load(ctx, &cff.local_index, base, (uint32_t)len, cff.local_index_offset);
		cff.subr_bias = subr_bias(ctx, &cff, cff.local_index.count);
		index_load(ctx, &cff.fdarray_index, base, (uint32_t)len, cff.fdarray_index_offset);

		get_encoding_len(ctx, &cff);
		get_charset_len(ctx, &cff);

		if (is_pdf_cidfont && cff.is_cidfont)
		{
			read_fdselect(ctx, &cff);
			read_fdarray_and_privates(ctx, &cff);
		}

		/* Move our list of gids into our own storage. */
		if (is_pdf_cidfont && cff.is_cidfont)
		{
			/* For CIDFontType0 FontDescriptor with a CFF that uses CIDFont operators,
			 * we are given CIDs here, not GIDs. Accordingly
			 * we need to look them up in the CharSet.
			 */
			load_charset_for_cidfont(ctx, &cff);
			for (i = 0; i < num_gids; i++)
				usage_list_add(ctx, &cff.gids_to_keep, cid_to_gid(ctx, &cff, gids[i]));
		}
		else
		{
			/* For CIDFontType0 FontDescriptor with a CFF that DOES NOT use CIDFont operators,
			 * and for Type1 FontDescriptors, we are given GIDs directly.
			 */
			for (i = 0; i < num_gids; i++)
				usage_list_add(ctx, &cff.gids_to_keep, gids[i]);
		}

		/* Scan charstrings. */
		scan_charstrings(ctx, &cff, is_pdf_cidfont);

		/* Now subset the data. */
		subset_charstrings(ctx, &cff);
		if (is_pdf_cidfont && cff.is_cidfont)
			subset_fdarray_locals(ctx, &cff);
		subset_locals(ctx, &cff);
		subset_globals(ctx, &cff);

		/* FIXME: cull the strings? */

		/*	Now, rewrite the font.

			There are various sections for this, as follows:

				SECTION			CIDFonts	Dict
					(Subsection)	only		Contains
									absolute
									offsets?
				Header
				Name Index
				Top Dict Index
					(Top Dict)			Y
				String Index
				Global Subr Index
				Encodings
				Charsets
				FDSelect		Y
				CharStrings Index
				Font DICT Index		Y
					(Font Dict)			N
				Private DICT				N
				Local Subr Index

		The size of global offsets varies according to how large the file is,
		therefore we need to take care.

		The 'suffix' of sections from String Index onwards are independent of
		this global offset size, so we finalise those sections first.

		We can then use this size to inform our choice of offset size for the
		top dictionary.

		So, layout the sections from the end backwards.
		*/

		/* Local Subr Index */
		/* Private DICT */
		make_new_private_dict(ctx, &cff);
		/* Font DICT - CIDFont only */
		/* Charstrings - already done */
		/* FDSelect - CIDFont only */
		/* Charsets - unchanged */
		/* Encoding - unchanged */
		/* Globals */
		/* Strings - unchanged */
		make_new_top_dict(ctx, &cff);

		newbuf = fz_new_buffer(ctx, 1024);
		out = fz_new_output_with_buffer(ctx, newbuf);

		/* Copy header */
		fz_write_byte(ctx, out, cff.major);
		fz_write_byte(ctx, out, cff.minor);
		fz_write_byte(ctx, out, 4);
		fz_write_byte(ctx, out, cff.offsize);

		output_name_index(ctx, &cff, out);
		update_dicts(ctx, &cff, fz_tell_output(ctx, out));
		output_top_dict_index(ctx, &cff, out);

		/* Copy strings index */
		if (cff.string_index.index_size)
			fz_write_data(ctx, out, base + cff.string_index.index_offset, cff.string_index.index_size);
		else
			fz_write_uint16_be(ctx, out, 0);
		/* Copy globals index (if there is one) */
		if (cff.global_subset)
			fz_write_data(ctx, out, cff.global_subset->data, cff.global_subset->len);
		else if (cff.global_index.index_size)
			fz_write_data(ctx, out, base + cff.global_index.index_offset, cff.global_index.index_size);
		else
			fz_write_uint16_be(ctx, out, 0);
		/* Copy encoding */
		if (cff.encoding_offset > 2)
			fz_write_data(ctx, out, base + cff.encoding_offset, cff.encoding_len);
		/* Copy charset */
		if (cff.charset_offset > 2)
			fz_write_data(ctx, out, base + cff.charset_offset, cff.charset_len);
		if (cff.fdselect_offset)
			fz_write_data(ctx, out, base + cff.fdselect_offset, cff.fdselect_len);
		/* Copy charstrings */
		if (cff.charstrings_subset)
			fz_write_data(ctx, out, cff.charstrings_subset->data, cff.charstrings_subset->len);
		else if (cff.charstrings_index.index_size)
			fz_write_data(ctx, out, base + cff.charstrings_index.index_offset, cff.charstrings_index.index_size);
		else
			fz_write_uint16_be(ctx, out, 0);
		if (cff.fdarray)
			output_fdarray(ctx, out, &cff);
		/* Copy Private dict */
		fz_write_data(ctx, out, cff.private_subset->data, cff.private_subset->len);
		/* Copy the local table - subsetted if there is one, original if not, or maybe none! */
		if (cff.local_subset)
			fz_write_data(ctx, out, cff.local_subset->data, cff.local_subset->len);
		else if (cff.local_index.index_size)
			fz_write_data(ctx, out, base + cff.local_index.index_offset, cff.local_index.index_size);

		fz_close_output(ctx, out);
	}
	fz_always(ctx)
	{
		fz_drop_output(ctx, out);
		fz_drop_buffer(ctx, cff.private_subset);
		fz_drop_buffer(ctx, cff.charstrings_subset);
		fz_drop_buffer(ctx, cff.top_dict_subset);
		fz_drop_buffer(ctx, cff.local_subset);
		fz_drop_buffer(ctx, cff.global_subset);
		fz_free(ctx, cff.gid_to_cid);
		fz_free(ctx, cff.gid_to_font);
		drop_usage_list(ctx, &cff.local_usage);
		drop_usage_list(ctx, &cff.global_usage);
		drop_usage_list(ctx, &cff.gids_to_keep);
		drop_usage_list(ctx, &cff.extra_gids_to_keep);
		if (cff.fdarray)
		{
			n = cff.fdarray_index.count;
			for (k = 0; k < n; k++)
			{
				fz_drop_buffer(ctx, cff.fdarray[k].rewritten_dict);
				fz_drop_buffer(ctx, cff.fdarray[k].rewritten_private);
				fz_drop_buffer(ctx, cff.fdarray[k].local_subset);
				drop_usage_list(ctx, &cff.fdarray[k].local_usage);
			}
			fz_free(ctx, cff.fdarray);
		}
		fz_free(ctx, cff.unpacked_charset);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, newbuf);
		fz_rethrow(ctx);
	}

	return newbuf;
}
