/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/


/* An implementation of MMR decoding. This is based on the
   implementation in Fitz, which in turn is based on the one
   in Ghostscript.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_arith.h"
#include "jbig2_generic.h"
#include "jbig2_mmr.h"

typedef struct {
	int width;
	int height;
	const byte *data;
	size_t size;
	int data_index;
	int bit_index;
	uint32_t word;
} Jbig2MmrCtx;


static void
jbig2_decode_mmr_init(Jbig2MmrCtx *mmr, int width, int height, const byte *data, size_t size)
{
	int i;
	uint32_t word = 0;

	mmr->width = width;
	mmr->height = height;
	mmr->data = data;
	mmr->size = size;
	mmr->data_index = 0;
	mmr->bit_index = 0;

	for (i = 0; i < size && i < 4; i++)
		word |= (data[i] << ((3 - i) << 3));
	mmr->word = word;
}

static void
jbig2_decode_mmr_consume(Jbig2MmrCtx *mmr, int n_bits)
{
	mmr->word <<= n_bits;
	mmr->bit_index += n_bits;
	while (mmr->bit_index >= 8) {
		mmr->bit_index -= 8;
		if (mmr->data_index + 4 < mmr->size)
			mmr->word |= (mmr->data[mmr->data_index + 4] << mmr->bit_index);
		mmr->data_index++;
	}
}

/*
<raph> the first 2^(initialbits) entries map bit patterns to decodes
<raph> let's say initial_bits is 8 for the sake of example
<raph> and that the code is 1001
<raph> that means that entries 0x90 .. 0x9f have the entry { val, 4 }
<raph> because those are all the bytes that start with the code
<raph> and the 4 is the length of the code
... if (n_bits > initial_bits) ...
<raph> anyway, in that case, it basically points to a mini table
<raph> the n_bits is the maximum length of all codes beginning with that byte
<raph> so 2^(n_bits - initial_bits) is the size of the mini-table
<raph> peter came up with this, and it makes sense
*/

typedef struct {
	short val;
	short n_bits;
} mmr_table_node;

/* white decode table (runlength huffman codes) */
const mmr_table_node jbig2_mmr_white_decode[] = {
{ 256, 12 },
{ 272, 12 },
{ 29, 8 },
{ 30, 8 },
{ 45, 8 },
{ 46, 8 },
{ 22, 7 },
{ 22, 7 },
{ 23, 7 },
{ 23, 7 },
{ 47, 8 },
{ 48, 8 },
{ 13, 6 },
{ 13, 6 },
{ 13, 6 },
{ 13, 6 },
{ 20, 7 },
{ 20, 7 },
{ 33, 8 },
{ 34, 8 },
{ 35, 8 },
{ 36, 8 },
{ 37, 8 },
{ 38, 8 },
{ 19, 7 },
{ 19, 7 },
{ 31, 8 },
{ 32, 8 },
{ 1, 6 },
{ 1, 6 },
{ 1, 6 },
{ 1, 6 },
{ 12, 6 },
{ 12, 6 },
{ 12, 6 },
{ 12, 6 },
{ 53, 8 },
{ 54, 8 },
{ 26, 7 },
{ 26, 7 },
{ 39, 8 },
{ 40, 8 },
{ 41, 8 },
{ 42, 8 },
{ 43, 8 },
{ 44, 8 },
{ 21, 7 },
{ 21, 7 },
{ 28, 7 },
{ 28, 7 },
{ 61, 8 },
{ 62, 8 },
{ 63, 8 },
{ 0, 8 },
{ 320, 8 },
{ 384, 8 },
{ 10, 5 },
{ 10, 5 },
{ 10, 5 },
{ 10, 5 },
{ 10, 5 },
{ 10, 5 },
{ 10, 5 },
{ 10, 5 },
{ 11, 5 },
{ 11, 5 },
{ 11, 5 },
{ 11, 5 },
{ 11, 5 },
{ 11, 5 },
{ 11, 5 },
{ 11, 5 },
{ 27, 7 },
{ 27, 7 },
{ 59, 8 },
{ 60, 8 },
{ 288, 9 },
{ 290, 9 },
{ 18, 7 },
{ 18, 7 },
{ 24, 7 },
{ 24, 7 },
{ 49, 8 },
{ 50, 8 },
{ 51, 8 },
{ 52, 8 },
{ 25, 7 },
{ 25, 7 },
{ 55, 8 },
{ 56, 8 },
{ 57, 8 },
{ 58, 8 },
{ 192, 6 },
{ 192, 6 },
{ 192, 6 },
{ 192, 6 },
{ 1664, 6 },
{ 1664, 6 },
{ 1664, 6 },
{ 1664, 6 },
{ 448, 8 },
{ 512, 8 },
{ 292, 9 },
{ 640, 8 },
{ 576, 8 },
{ 294, 9 },
{ 296, 9 },
{ 298, 9 },
{ 300, 9 },
{ 302, 9 },
{ 256, 7 },
{ 256, 7 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 2, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 3, 4 },
{ 128, 5 },
{ 128, 5 },
{ 128, 5 },
{ 128, 5 },
{ 128, 5 },
{ 128, 5 },
{ 128, 5 },
{ 128, 5 },
{ 8, 5 },
{ 8, 5 },
{ 8, 5 },
{ 8, 5 },
{ 8, 5 },
{ 8, 5 },
{ 8, 5 },
{ 8, 5 },
{ 9, 5 },
{ 9, 5 },
{ 9, 5 },
{ 9, 5 },
{ 9, 5 },
{ 9, 5 },
{ 9, 5 },
{ 9, 5 },
{ 16, 6 },
{ 16, 6 },
{ 16, 6 },
{ 16, 6 },
{ 17, 6 },
{ 17, 6 },
{ 17, 6 },
{ 17, 6 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 4, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 14, 6 },
{ 14, 6 },
{ 14, 6 },
{ 14, 6 },
{ 15, 6 },
{ 15, 6 },
{ 15, 6 },
{ 15, 6 },
{ 64, 5 },
{ 64, 5 },
{ 64, 5 },
{ 64, 5 },
{ 64, 5 },
{ 64, 5 },
{ 64, 5 },
{ 64, 5 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ 7, 4 },
{ -2, 3 },
{ -2, 3 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -3, 4 },
{ 1792, 3 },
{ 1792, 3 },
{ 1984, 4 },
{ 2048, 4 },
{ 2112, 4 },
{ 2176, 4 },
{ 2240, 4 },
{ 2304, 4 },
{ 1856, 3 },
{ 1856, 3 },
{ 1920, 3 },
{ 1920, 3 },
{ 2368, 4 },
{ 2432, 4 },
{ 2496, 4 },
{ 2560, 4 },
{ 1472, 1 },
{ 1536, 1 },
{ 1600, 1 },
{ 1728, 1 },
{ 704, 1 },
{ 768, 1 },
{ 832, 1 },
{ 896, 1 },
{ 960, 1 },
{ 1024, 1 },
{ 1088, 1 },
{ 1152, 1 },
{ 1216, 1 },
{ 1280, 1 },
{ 1344, 1 },
{ 1408, 1 }
};

/* black decode table (runlength huffman codes) */
const mmr_table_node jbig2_mmr_black_decode[] = {
{ 128, 12 },
{ 160, 13 },
{ 224, 12 },
{ 256, 12 },
{ 10, 7 },
{ 11, 7 },
{ 288, 12 },
{ 12, 7 },
{ 9, 6 },
{ 9, 6 },
{ 8, 6 },
{ 8, 6 },
{ 7, 5 },
{ 7, 5 },
{ 7, 5 },
{ 7, 5 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 6, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 5, 4 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 1, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 4, 3 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 3, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ 2, 2 },
{ -2, 4 },
{ -2, 4 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -1, 0 },
{ -3, 5 },
{ 1792, 4 },
{ 1792, 4 },
{ 1984, 5 },
{ 2048, 5 },
{ 2112, 5 },
{ 2176, 5 },
{ 2240, 5 },
{ 2304, 5 },
{ 1856, 4 },
{ 1856, 4 },
{ 1920, 4 },
{ 1920, 4 },
{ 2368, 5 },
{ 2432, 5 },
{ 2496, 5 },
{ 2560, 5 },
{ 18, 3 },
{ 18, 3 },
{ 18, 3 },
{ 18, 3 },
{ 18, 3 },
{ 18, 3 },
{ 18, 3 },
{ 18, 3 },
{ 52, 5 },
{ 52, 5 },
{ 640, 6 },
{ 704, 6 },
{ 768, 6 },
{ 832, 6 },
{ 55, 5 },
{ 55, 5 },
{ 56, 5 },
{ 56, 5 },
{ 1280, 6 },
{ 1344, 6 },
{ 1408, 6 },
{ 1472, 6 },
{ 59, 5 },
{ 59, 5 },
{ 60, 5 },
{ 60, 5 },
{ 1536, 6 },
{ 1600, 6 },
{ 24, 4 },
{ 24, 4 },
{ 24, 4 },
{ 24, 4 },
{ 25, 4 },
{ 25, 4 },
{ 25, 4 },
{ 25, 4 },
{ 1664, 6 },
{ 1728, 6 },
{ 320, 5 },
{ 320, 5 },
{ 384, 5 },
{ 384, 5 },
{ 448, 5 },
{ 448, 5 },
{ 512, 6 },
{ 576, 6 },
{ 53, 5 },
{ 53, 5 },
{ 54, 5 },
{ 54, 5 },
{ 896, 6 },
{ 960, 6 },
{ 1024, 6 },
{ 1088, 6 },
{ 1152, 6 },
{ 1216, 6 },
{ 64, 3 },
{ 64, 3 },
{ 64, 3 },
{ 64, 3 },
{ 64, 3 },
{ 64, 3 },
{ 64, 3 },
{ 64, 3 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 13, 1 },
{ 23, 4 },
{ 23, 4 },
{ 50, 5 },
{ 51, 5 },
{ 44, 5 },
{ 45, 5 },
{ 46, 5 },
{ 47, 5 },
{ 57, 5 },
{ 58, 5 },
{ 61, 5 },
{ 256, 5 },
{ 16, 3 },
{ 16, 3 },
{ 16, 3 },
{ 16, 3 },
{ 17, 3 },
{ 17, 3 },
{ 17, 3 },
{ 17, 3 },
{ 48, 5 },
{ 49, 5 },
{ 62, 5 },
{ 63, 5 },
{ 30, 5 },
{ 31, 5 },
{ 32, 5 },
{ 33, 5 },
{ 40, 5 },
{ 41, 5 },
{ 22, 4 },
{ 22, 4 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 14, 1 },
{ 15, 2 },
{ 15, 2 },
{ 15, 2 },
{ 15, 2 },
{ 15, 2 },
{ 15, 2 },
{ 15, 2 },
{ 15, 2 },
{ 128, 5 },
{ 192, 5 },
{ 26, 5 },
{ 27, 5 },
{ 28, 5 },
{ 29, 5 },
{ 19, 4 },
{ 19, 4 },
{ 20, 4 },
{ 20, 4 },
{ 34, 5 },
{ 35, 5 },
{ 36, 5 },
{ 37, 5 },
{ 38, 5 },
{ 39, 5 },
{ 21, 4 },
{ 21, 4 },
{ 42, 5 },
{ 43, 5 },
{ 0, 3 },
{ 0, 3 },
{ 0, 3 },
{ 0, 3 }
};

#define getbit(buf, x) ( ( buf[x >> 3] >> ( 7 - (x & 7) ) ) & 1 )

static int
jbig2_find_changing_element(const byte *line, int x, int w)
{
	int a, b;

	if (line == 0)
		return w;

	if (x == -1) {
		a = 0;
		x = 0;
	}
	else {
		a = getbit(line, x);
		x ++;
	}

	while (x < w) {
		b = getbit(line, x);
		if (a != b)
			break;
		x++;
	}

	return x;
}

static int
jbig2_find_changing_element_of_color(const byte *line, int x, int w, int color)
{
	if (line == 0)
		return w;
	x = jbig2_find_changing_element(line, x, w);
	if (x < w && getbit(line, x) != color)
		x = jbig2_find_changing_element(line, x, w);
	return x;
}

static const byte lm[8] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01 };
static const byte rm[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };

static void
jbig2_set_bits(byte *line, int x0, int x1)
{
	int a0, a1, b0, b1, a;

	a0 = x0 >> 3;
	a1 = x1 >> 3;

	b0 = x0 & 7;
	b1 = x1 & 7;

	if (a0 == a1) {
		line[a0] |= lm[b0] & rm[b1];
	}
	else {
		line[a0] |= lm[b0];
		for (a = a0 + 1; a < a1; a++)
			line[a] = 0xFF;
		if (b1) line[a1] |= rm[b1];
	}
}


static int
jbig2_decode_get_code(Jbig2MmrCtx *mmr, const mmr_table_node *table, int initial_bits)
{
	uint32_t word = mmr->word;
	int table_ix = word >> (32 - initial_bits);
	int val = table[table_ix].val;
	int n_bits = table[table_ix].n_bits;

	if (n_bits > initial_bits) {
		int mask = (1 << (32 - initial_bits)) - 1;
		table_ix = val + ((word & mask) >> (32 - n_bits));
		val = table[table_ix].val;
		n_bits = initial_bits + table[table_ix].n_bits;
	}

	jbig2_decode_mmr_consume(mmr, n_bits);

	return val;
}

static int
jbig2_decode_get_run(Jbig2MmrCtx *mmr, const mmr_table_node *table, int initial_bits)
{
	int result = 0;
	int val;

	do {
		val = jbig2_decode_get_code(mmr, table, initial_bits);
		result += val;
	} while (val >= 64);

	return result;
}

static int
jbig2_decode_mmr_line(Jbig2MmrCtx *mmr, const byte *ref, byte *dst)
{
	int a0 = -1;
	int a1, a2, b1, b2;
	int c = 0;		/* 0 is white, black is 1 */

	while (1)
	{
		uint32_t word = mmr->word;
		/* printf ("%08x\n", word); */

		if (a0 >= mmr->width)
			break;

		if ((word >> (32 - 3)) == 1)
		{
			int white_run, black_run;

			jbig2_decode_mmr_consume(mmr, 3);

			if (a0 == -1)
				a0 = 0;

			if (c == 0) {
				white_run = jbig2_decode_get_run(mmr, jbig2_mmr_white_decode, 8);
				black_run = jbig2_decode_get_run(mmr, jbig2_mmr_black_decode, 7);
				a1 = a0 + white_run;
				a2 = a1 + black_run;
				if (a1 > mmr->width) a1 = mmr->width;
				if (a2 > mmr->width) a2 = mmr->width;
				if (a2 < a1) return -1;
				jbig2_set_bits(dst, a1, a2);
				a0 = a2;
				/* printf ("H %d %d\n", white_run, black_run); */
			}
			else
			{
				black_run = jbig2_decode_get_run(mmr, jbig2_mmr_black_decode, 7);
				white_run = jbig2_decode_get_run(mmr, jbig2_mmr_white_decode, 8);
				a1 = a0 + black_run;
				a2 = a1 + white_run;
				if (a1 > mmr->width) a1 = mmr->width;
				if (a2 > mmr->width) a2 = mmr->width;
				if (a1 < a0) return -1;
				jbig2_set_bits(dst, a0, a1);
				a0 = a2;
				/* printf ("H %d %d\n", black_run, white_run); */
			}
		}

		else if ((word >> (32 - 4)) == 1)
		{
			/* printf ("P\n"); */
			jbig2_decode_mmr_consume(mmr, 4);
			b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
			b2 = jbig2_find_changing_element(ref, b1, mmr->width);
			if (c)
			{
				if (b2 < a0) return -1;
				jbig2_set_bits(dst, a0, b2);
			}
			a0 = b2;
		}

		else if ((word >> (32 - 1)) == 1)
		{
			/* printf ("V(0)\n"); */
			jbig2_decode_mmr_consume(mmr, 1);
			b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
			if (c)
			{
				if (b1 < a0) return -1;
				jbig2_set_bits(dst, a0, b1);
			}
			a0 = b1;
			c = !c;
		}

		else if ((word >> (32 - 3)) == 3)
		{
			/* printf ("VR(1)\n"); */
			jbig2_decode_mmr_consume(mmr, 3);
			b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
			if (b1 + 1 > mmr->width) break;
			if (c)
			{
				if (b1 + 1 < a0) return -1;
				jbig2_set_bits(dst, a0, b1 + 1);
			}
			a0 = b1 + 1;
			c = !c;
		}

		else if ((word >> (32 - 6)) == 3)
		{
			/* printf ("VR(2)\n"); */
			jbig2_decode_mmr_consume(mmr, 6);
			b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
			if (b1 + 2 > mmr->width) break;
			if (c)
			{
				if (b1 + 2 < a0) return -1;
				jbig2_set_bits(dst, a0, b1 + 2);
			}
			a0 = b1 + 2;
			c = !c;
		}

		else if ((word >> (32 - 7)) == 3)
		{
			/* printf ("VR(3)\n"); */
			jbig2_decode_mmr_consume(mmr, 7);
			b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
			if (b1 + 3 > mmr->width) break;
			if (c)
			{
				if (b1 + 3 < a0) return -1;
				jbig2_set_bits(dst, a0, b1 + 3);
			}
			a0 = b1 + 3;
			c = !c;
		}

		else if ((word >> (32 - 3)) == 2)
		{
			/* printf ("VL(1)\n"); */
			jbig2_decode_mmr_consume(mmr, 3);
			b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
			if (b1 - 1 < 0) break;
			if (c)
			{
				if (b1 - 1 < a0) return -1;
				jbig2_set_bits(dst, a0, b1 - 1);
			}
			a0 = b1 - 1;
			c = !c;
		}

		else if ((word >> (32 - 6)) == 2)
		{
			/* printf ("VL(2)\n"); */
			jbig2_decode_mmr_consume(mmr, 6);
			b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
			if (b1 - 2 < 0) break;
			if (c)
			{
				if (b1 - 2 < a0) return -1;
				jbig2_set_bits(dst, a0, b1 - 2);
			}
			a0 = b1 - 2;
			c = !c;
		}

		else if ((word >> (32 - 7)) == 2)
		{
			/* printf ("VL(3)\n"); */
			jbig2_decode_mmr_consume(mmr, 7);
			b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
			if (b1 - 3 < 0) break;
			if (c)
			{
				if (b1 - 3 < a0) return -1;
				jbig2_set_bits(dst, a0, b1 - 3);
			}
			a0 = b1 - 3;
			c = !c;
		}

		else
			break;
	}

	return 0;
}

int
jbig2_decode_generic_mmr(Jbig2Ctx *ctx,
	Jbig2Segment *segment,
	const Jbig2GenericRegionParams *params,
	const byte *data, size_t size,
	Jbig2Image *image)
{
	Jbig2MmrCtx mmr;
	const int rowstride = image->stride;
	byte *dst = image->data;
	byte *ref = NULL;
	int y;
	int code = 0;

	jbig2_decode_mmr_init(&mmr, image->width, image->height, data, size);

	for (y = 0; y < image->height; y++) {
		memset(dst, 0, rowstride);
		code = jbig2_decode_mmr_line(&mmr, ref, dst);
		if (code < 0) return code;
		ref = dst;
		dst += rowstride;
	}

	return code;
}

/**
 * jbig2_decode_halftone_mmr: decode mmr region inside of halftones
 * 
 * @ctx: jbig2 decoder context
 * @params: parameters for decoding  
 * @data: pointer to text region data to be decoded
 * @size: length of text region data
 * @image: return of decoded image 
 * @consumed_bytes: return of consumed bytes from @data
 *
 * MMR decoding that consumes EOFB and returns consumed bytes (@consumed_bytes)
 *
 * returns: 0 
 **/
int
jbig2_decode_halftone_mmr(Jbig2Ctx *ctx,
	const Jbig2GenericRegionParams *params,
	const byte *data, size_t size,
	Jbig2Image *image, size_t* consumed_bytes)
{
	Jbig2MmrCtx mmr;
	const int rowstride = image->stride;
	byte *dst = image->data;
	byte *ref = NULL;
	int y;
        int code = 0;
	const uint32_t EOFB = 0x001001;

	jbig2_decode_mmr_init(&mmr, image->width, image->height, data, size);

	for (y = 0; y < image->height; y++) {
		memset(dst, 0, rowstride);
		code = jbig2_decode_mmr_line(&mmr, ref, dst);
               if (code < 0) return code;
		ref = dst;
		dst += rowstride;
	}

	/* test for EOFB (see section 6.2.6) */
	if (mmr.word >> 8 == EOFB) {
		mmr.data_index += 3;
	}

	*consumed_bytes += mmr.data_index + (mmr.bit_index >> 3) + 
                       (mmr.bit_index > 0 ? 1 : 0);
	return code;
}
