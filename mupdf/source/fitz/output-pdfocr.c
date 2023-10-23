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
#include <string.h>
#include <limits.h>

#ifdef OCR_DISABLED

/* In non-OCR builds, we need to define this otherwise SWIG Python gets SEGV
when it attempts to import mupdf.py and _mupdf.py. */
const char *fz_pdfocr_write_options_usage = "";

#else

#include "tessocr.h"

const char *fz_pdfocr_write_options_usage =
	"PDFOCR output options:\n"
	"\tcompression=none: No compression (default)\n"
	"\tcompression=flate: Flate compression\n"
	"\tstrip-height=N: Strip height (default 0=fullpage)\n"
	"\tocr-language=<lang>: OCR language (default=eng)\n"
	"\tocr-datadir=<datadir>: OCR data path (default=rely on TESSDATA_PREFIX)\n"
	"\n";

static const char funky_font[] =
"3 0 obj\n<</BaseFont/GlyphLessFont/DescendantFonts[4 0 R]"
"/Encoding/Identity-H/Subtype/Type0/ToUnicode 6 0 R/Type/Font"
">>\nendobj\n";

static const char funky_font2[] =
"4 0 obj\n"
"<</BaseFont/GlyphLessFont/CIDToGIDMap 5 0 R"
"/CIDSystemInfo<</Ordering (Identity)/Registry (Adobe)/Supplement 0>>"
"/FontDescriptor 7 0 R/Subtype/CIDFontType2/Type/Font/DW 500>>"
"\nendobj\n";

static const char funky_font3[] =
"5 0 obj\n<</Length 210/Filter/FlateDecode>>\nstream\n"
"\x78\x9c\xec\xc2\x01\x09\x00\x00\x00\x02\xa0\xfa\x7f\xba\x21\x89"
"\xa6\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
"\x80\x7b\x03\x00\x00\xff\xff\xec\xc2\x01\x0d\x00\x00\x00\xc2\x20"
"\xdf\xbf\xb4\x45\x18\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\xeb\x00\x00\x00\xff\xff\xec\xc2\x01\x0d\x00"
"\x00\x00\xc2\x20\xdf\xbf\xb4\x45\x18\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\xeb\x00\x00\x00\xff\xff\xed"
"\xc2\x01\x0d\x00\x00\x00\xc2\x20\xdf\xbf\xb4\x45\x18\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xeb\x00\xff"
"\x00\x10"
"\nendstream\nendobj\n";

static const char funky_font4[] =
"6 0 obj\n<</Length 353>>\nstream\n"
"/CIDInit /ProcSet findresource begin\n"
"12 dict begin\n"
"begincmap\n"
"/CIDSystemInfo\n"
"<<\n"
"  /Registry (Adobe)\n"
"  /Ordering (UCS)\n"
"  /Supplement 0\n"
">> def\n"
"/CMapName /Adobe-Identity-UCS def\n"
"/CMapType 2 def\n"
"1 begincodespacerange\n"
"<0000> <FFFF>\n"
"endcodespacerange\n"
"1 beginbfrange\n"
"<0000> <FFFF> <0000>\n"
"endbfrange\n"
"endcmap\n"
"CMapName currentdict /CMap defineresource pop\n"
"end\n"
"end\n"
"endstream\n"
"endobj\n";

static const char funky_font5[] =
"7 0 obj\n"
"<</Ascent 1000/CapHeight 1000/Descent -1/Flags 5"
"/FontBBox[0 0 500 1000]/FontFile2 8 0 R/FontName/GlyphLessFont"
"/ItalicAngle 0/StemV 80/Type/FontDescriptor>>\nendobj\n";

static const char funky_font6[] =
"8 0 obj\n<</Length 572/Length1 572>>\nstream\n"
"\x00\x01\x00\x00\x00\x0a\x00\x80\x00\x03\x00\x20\x4f\x53\x2f\x32"
"\x56\xde\xc8\x94\x00\x00\x01\x28\x00\x00\x00\x60\x63\x6d\x61\x70"
"\x00\x0a\x00\x34\x00\x00\x01\x90\x00\x00\x00\x1e\x67\x6c\x79\x66"
"\x15\x22\x41\x24\x00\x00\x01\xb8\x00\x00\x00\x18\x68\x65\x61\x64"
"\x0b\x78\xf1\x65\x00\x00\x00\xac\x00\x00\x00\x36\x68\x68\x65\x61"
"\x0c\x02\x04\x02\x00\x00\x00\xe4\x00\x00\x00\x24\x68\x6d\x74\x78"
"\x04\x00\x00\x00\x00\x00\x01\x88\x00\x00\x00\x08\x6c\x6f\x63\x61"
"\x00\x0c\x00\x00\x00\x00\x01\xb0\x00\x00\x00\x06\x6d\x61\x78\x70"
"\x00\x04\x00\x05\x00\x00\x01\x08\x00\x00\x00\x20\x6e\x61\x6d\x65"
"\xf2\xeb\x16\xda\x00\x00\x01\xd0\x00\x00\x00\x4b\x70\x6f\x73\x74"
"\x00\x01\x00\x01\x00\x00\x02\x1c\x00\x00\x00\x20\x00\x01\x00\x00"
"\x00\x01\x00\x00\xb0\x94\x71\x10\x5f\x0f\x3c\xf5\x04\x07\x08\x00"
"\x00\x00\x00\x00\xcf\x9a\xfc\x6e\x00\x00\x00\x00\xd4\xc3\xa7\xf2"
"\x00\x00\x00\x00\x04\x00\x08\x00\x00\x00\x00\x10\x00\x02\x00\x00"
"\x00\x00\x00\x00\x00\x01\x00\x00\x08\x00\xff\xff\x00\x00\x04\x00"
"\x00\x00\x00\x00\x04\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x02\x00\x01\x00\x00\x00\x02\x00\x04"
"\x00\x01\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x03\x00\x00\x01\x90\x00\x05"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x05\x00\x01\x00\x01\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x47\x4f\x4f\x47\x00\x40\x00\x00\x00\x00\x00\x01\xff\xff"
"\x00\x00\x00\x01\x00\x01\x80\x00\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x04\x00\x00\x00"
"\x00\x00\x00\x02\x00\x01\x00\x00\x00\x00\x00\x14\x00\x03\x00\x00"
"\x00\x00\x00\x14\x00\x06\x00\x0a\x00\x00\x00\x00\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x0c\x00\x00\x00\x01\x00\x00\x00\x00\x04\x00"
"\x08\x00\x00\x03\x00\x00\x31\x21\x11\x21\x04\x00\xfc\x00\x08\x00"
"\x00\x00\x00\x03\x00\x2a\x00\x00\x00\x03\x00\x00\x00\x05\x00\x16"
"\x00\x00\x00\x01\x00\x00\x00\x00\x00\x05\x00\x0b\x00\x16\x00\x03"
"\x00\x01\x04\x09\x00\x05\x00\x16\x00\x00\x00\x56\x00\x65\x00\x72"
"\x00\x73\x00\x69\x00\x6f\x00\x6e\x00\x20\x00\x31\x00\x2e\x00\x30"
"\x56\x65\x72\x73\x69\x6f\x6e\x20\x31\x2e\x30\x00\x00\x01\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00"
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
"\nendstream\nendobj\n";

#endif

fz_pdfocr_options *
fz_parse_pdfocr_options(fz_context *ctx, fz_pdfocr_options *opts, const char *args)
{
#ifdef OCR_DISABLED
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OCR support in this build");
#else
	const char *val;

	memset(opts, 0, sizeof *opts);

	if (fz_has_option(ctx, args, "compression", &val))
	{
		if (fz_option_eq(val, "none"))
			opts->compress = 0;
		else if (fz_option_eq(val, "flate"))
			opts->compress = 1;
		else
			fz_throw(ctx, FZ_ERROR_GENERIC, "Unsupported PDFOCR compression %s (none, or flate only)", val);
	}
	if (fz_has_option(ctx, args, "strip-height", &val))
	{
		int i = fz_atoi(val);
		if (i <= 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "Unsupported PDFOCR strip height %d (suggest 0)", i);
		opts->strip_height = i;
	}
	if (fz_has_option(ctx, args, "ocr-language", &val))
	{
		fz_copy_option(ctx, val, opts->language, nelem(opts->language));
	}
	if (fz_has_option(ctx, args, "ocr-datadir", &val))
	{
		fz_copy_option(ctx, val, opts->datadir, nelem(opts->datadir));
	}

	return opts;
#endif
}

void
fz_write_pixmap_as_pdfocr(fz_context *ctx, fz_output *out, const fz_pixmap *pixmap, const fz_pdfocr_options *pdfocr)
{
#ifdef OCR_DISABLED
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OCR support in this build");
#else
	fz_band_writer *writer;

	if (!pixmap || !out)
		return;

	writer = fz_new_pdfocr_band_writer(ctx, out, pdfocr);
	fz_try(ctx)
	{
		fz_write_header(ctx, writer, pixmap->w, pixmap->h, pixmap->n, pixmap->alpha, pixmap->xres, pixmap->yres, 0, pixmap->colorspace, pixmap->seps);
		fz_write_band(ctx, writer, pixmap->stride, pixmap->h, pixmap->samples);
		fz_close_band_writer(ctx, writer);
	}
	fz_always(ctx)
		fz_drop_band_writer(ctx, writer);
	fz_catch(ctx)
		fz_rethrow(ctx);
#endif
}

#ifndef OCR_DISABLED
typedef struct pdfocr_band_writer_s
{
	fz_band_writer super;
	fz_pdfocr_options options;

	int obj_num;
	int xref_max;
	int64_t *xref;
	int pages;
	int page_max;
	int *page_obj;
	unsigned char *stripbuf;
	unsigned char *compbuf;
	size_t complen;

	void *tessapi;
	fz_pixmap *ocrbitmap;

	fz_pdfocr_progress_fn *progress;
	void *progress_arg;
} pdfocr_band_writer;

static int
new_obj(fz_context *ctx, pdfocr_band_writer *writer)
{
	int64_t pos = fz_tell_output(ctx, writer->super.out);

	if (writer->obj_num >= writer->xref_max)
	{
		int new_max = writer->xref_max * 2;
		if (new_max < writer->obj_num + 8)
			new_max = writer->obj_num + 8;
		writer->xref = fz_realloc_array(ctx, writer->xref, new_max, int64_t);
		writer->xref_max = new_max;
	}

	writer->xref[writer->obj_num] = pos;

	return writer->obj_num++;
}

static void
pdfocr_write_header(fz_context *ctx, fz_band_writer *writer_, fz_colorspace *cs)
{
	pdfocr_band_writer *writer = (pdfocr_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int n = writer->super.n;
	int s = writer->super.s;
	int a = writer->super.alpha;
	int xres = writer->super.xres;
	int yres = writer->super.yres;
	int sh = writer->options.strip_height;
	int strips;
	int i;

	if (sh == 0)
		sh = h;
	assert(sh != 0 && "pdfocr_write_header() should not be given zero height input.");
	strips = (h + sh-1)/sh;

	if (a != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PDFOCR cannot write alpha channel");
	if (s != 0)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PDFOCR cannot write spot colors");
	if (n != 3 && n != 1)
		fz_throw(ctx, FZ_ERROR_GENERIC, "PDFOCR expected to be Grayscale or RGB");

	fz_free(ctx, writer->stripbuf);
	writer->stripbuf = NULL;
	fz_free(ctx, writer->compbuf);
	writer->compbuf = NULL;
	fz_drop_pixmap(ctx, writer->ocrbitmap);
	writer->ocrbitmap = NULL;
	writer->stripbuf = Memento_label(fz_malloc(ctx, (size_t)w * sh * n), "pdfocr_stripbuf");
	writer->complen = fz_deflate_bound(ctx, (size_t)w * sh * n);
	writer->compbuf = Memento_label(fz_malloc(ctx, writer->complen), "pdfocr_compbuf");
	/* Always round the width of ocrbitmap up to a multiple of 4. */
	writer->ocrbitmap = fz_new_pixmap(ctx, NULL, (w+3)&~3, h, NULL, 0);
	fz_set_pixmap_resolution(ctx, writer->ocrbitmap, xres, yres);

	/* Send the file header on the first page */
	if (writer->pages == 0)
	{
		fz_write_string(ctx, out, "%PDF-1.4\n%PDFOCR-1.0\n");

		if (writer->xref_max < 9)
		{
			int new_max = 9;
			writer->xref = fz_realloc_array(ctx, writer->xref, new_max, int64_t);
			writer->xref_max = new_max;
		}
		writer->xref[3] = fz_tell_output(ctx, out);
		fz_write_data(ctx, out, funky_font,  sizeof(funky_font)-1);
		writer->xref[4] = fz_tell_output(ctx, out);
		fz_write_data(ctx, out, funky_font2, sizeof(funky_font2)-1);
		writer->xref[5] = fz_tell_output(ctx, out);
		fz_write_data(ctx, out, funky_font3, sizeof(funky_font3)-1);
		writer->xref[6] = fz_tell_output(ctx, out);
		fz_write_data(ctx, out, funky_font4, sizeof(funky_font4)-1);
		writer->xref[7] = fz_tell_output(ctx, out);
		fz_write_data(ctx, out, funky_font5, sizeof(funky_font5)-1);
		writer->xref[8] = fz_tell_output(ctx, out);
		fz_write_data(ctx, out, funky_font6, sizeof(funky_font6)-1);
	}

	if (writer->page_max <= writer->pages)
	{
		int new_max = writer->page_max * 2;
		if (new_max == 0)
			new_max = writer->pages + 8;
		writer->page_obj = fz_realloc_array(ctx, writer->page_obj, new_max, int);
		writer->page_max = new_max;
	}
	writer->page_obj[writer->pages] = writer->obj_num;
	writer->pages++;

	/* Send the Page Object */
	fz_write_printf(ctx, out, "%d 0 obj\n<</Type/Page/Parent 2 0 R/Resources<</XObject<<", new_obj(ctx, writer));
	for (i = 0; i < strips; i++)
		fz_write_printf(ctx, out, "/I%d %d 0 R", i, writer->obj_num + i);
	fz_write_printf(ctx, out, ">>/Font<</F0 3 0 R>>>>/MediaBox[0 0 %g %g]/Contents %d 0 R>>\nendobj\n",
		w * 72.0f / xres, h * 72.0f / yres, writer->obj_num + strips);
}

static void
flush_strip(fz_context *ctx, pdfocr_band_writer *writer, int fill)
{
	unsigned char *data = writer->stripbuf;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int n = writer->super.n;
	size_t len = (size_t)w*n*fill;

	/* Buffer is full, compress it and write it. */
	if (writer->options.compress)
	{
		size_t destLen = writer->complen;
		fz_deflate(ctx, writer->compbuf, &destLen, data, len, FZ_DEFLATE_DEFAULT);
		len = destLen;
		data = writer->compbuf;
	}
	fz_write_printf(ctx, out, "%d 0 obj\n<</Width %d/ColorSpace/Device%s/Height %d%s/Subtype/Image",
		new_obj(ctx, writer), w, n == 1 ? "Gray" : "RGB", fill, writer->options.compress ? "/Filter/FlateDecode" : "");
	fz_write_printf(ctx, out, "/Length %zd/Type/XObject/BitsPerComponent 8>>\nstream\n", len);
	fz_write_data(ctx, out, data, len);
	fz_write_string(ctx, out, "\nendstream\nendobj\n");
}

static void
pdfocr_write_band(fz_context *ctx, fz_band_writer *writer_, int stride, int band_start, int band_height, const unsigned char *sp)
{
	pdfocr_band_writer *writer = (pdfocr_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int n = writer->super.n;
	int sh = writer->options.strip_height;
	int line;
	unsigned char *d = writer->ocrbitmap->samples;

	if (!out)
		return;

	if (sh == 0)
		sh = h;

	for (line = 0; line < band_height; line++)
	{
		int dstline = (band_start+line) % sh;
		memcpy(writer->stripbuf + (size_t)w*n*dstline,
			   sp + (size_t)line * w * n,
			   (size_t)w * n);
		if (dstline+1 == sh)
			flush_strip(ctx, writer, dstline+1);
	}

	if (band_start + band_height == h && h % sh != 0)
		flush_strip(ctx, writer, h % sh);

	/* Copy strip to ocrbitmap, converting if required. */
	d += band_start*w;
	if (n == 1)
	{
		int y;
		for (y = band_height; y > 0; y--)
		{
			memcpy(d, sp, w);
			if (writer->ocrbitmap->w - w)
				memset(d + w, 0, writer->ocrbitmap->w - w);
			d += writer->ocrbitmap->w;
		}
	}
	else
	{
		int x, y;
		for (y = band_height; y > 0; y--)
		{
			for (x = w; x > 0; x--)
			{
				*d++ = (sp[0] + 2*sp[1] + sp[2] + 2)>>2;
				sp += 3;
			}
			for (x = writer->ocrbitmap->w - w; x > 0; x--)
				*d++ = 0;
		}
	}
}

enum
{
	WORD_CONTAINS_L2R = 1,
	WORD_CONTAINS_R2L = 2,
	WORD_CONTAINS_T2B = 4,
	WORD_CONTAINS_B2T = 8
};

typedef struct word_t
{
	struct word_t *next;
	float bbox[4];
	int dirn;
	int len;
	int chars[1];
} word_t;

typedef struct
{
	fz_buffer *buf;
	pdfocr_band_writer *writer;

	/* We collate the current word into the following fields: */
	int word_max;
	int word_len;
	int *word_chars;
	float word_bbox[4];
	int word_dirn;
	int word_prev_char_bbox[4];

	/* When we finish a word, we try to add it to the line. If the
	 * word fits onto the end of the existing line, great. If not,
	 * we flush the entire line, and start a new one just with the
	 * new word. This enables us to output a whole line at once,
	 * which is beneficial to avoid jittering the font sizes
	 * up/down, which looks bad when we try to select text in the
	 * produced PDF. */
	word_t *line;
	word_t **line_tail;
	float line_bbox[4];
	int line_dirn;

	float cur_size;
	float cur_scale;
	float tx, ty;
} char_callback_data_t;

static void
flush_words(fz_context *ctx, char_callback_data_t *cb)
{
	float size;

	if (cb->line == NULL)
		return;

	if ((cb->line_dirn & (WORD_CONTAINS_T2B | WORD_CONTAINS_B2T)) != 0)
	{
		/* Vertical line */
	}
	else
	{
		/* Horizontal line */
		size = cb->line_bbox[3] - cb->line_bbox[1];

		if (size != 0 && size != cb->cur_size)
		{
			fz_append_printf(ctx, cb->buf, "/F0 %g Tf\n", size);
			cb->cur_size = size;
		}
		/* Guard against division by 0. This makes no difference to the
		 * actual calculation as if size is 0, word->bbox[2] == word->bbox[0]
		 * too. */
		if (size == 0)
			size = 1;
	}

	while (cb->line)
	{
		word_t *word = cb->line;
		float x, y;
		int i, len = word->len;
		float scale;

		if ((cb->line_dirn & (WORD_CONTAINS_T2B | WORD_CONTAINS_B2T)) != 0)
		{
			/* Contains vertical text. */
			size = (word->bbox[3] - word->bbox[1]) / len;
			if (size == 0)
				size = 1;
			if (size != cb->cur_size)
			{
				fz_append_printf(ctx, cb->buf, "/F0 %g Tf\n", size);
				cb->cur_size = size;
			}

			/* Set the scale so that our glyphs fill the line bbox. */
			scale = (cb->line_bbox[2] - cb->line_bbox[0]) / size * 200;
			if (scale != 0)
			{
				float letter_height = (word->bbox[3] - word->bbox[1]) / len;

				if (scale != cb->cur_scale)
				{
					fz_append_printf(ctx, cb->buf, "%d Tz\n", (int)scale);
					cb->cur_scale = scale;
				}

				for (i = 0; i < len; i++)
				{
					x = word->bbox[0];
					y = word->bbox[1] + letter_height * i;
					fz_append_printf(ctx, cb->buf, "%g %g Td\n", x-cb->tx, y-cb->ty);
					cb->tx = x;
					cb->ty = y;

					fz_append_printf(ctx, cb->buf, "<%04x>Tj\n", word->chars[i]);
				}
			}
		}
		else
		{
			scale = (word->bbox[2] - word->bbox[0]) / size / len * 200;
			if (scale != 0)
			{
				if (scale != cb->cur_scale)
				{
					fz_append_printf(ctx, cb->buf, "%d Tz\n", (int)scale);
					cb->cur_scale = scale;
				}

				if ((word->dirn & (WORD_CONTAINS_R2L | WORD_CONTAINS_L2R)) == WORD_CONTAINS_R2L)
				{
					/* Purely R2L text */
					x = word->bbox[0];
					y = cb->line_bbox[1];
					fz_append_printf(ctx, cb->buf, "%g %g Td\n", x-cb->tx, y-cb->ty);
					cb->tx = x;
					cb->ty = y;

					/* Tesseract has sent us R2L text in R2L order (i.e. in Logical order).
					 * We want to output it in that same logical order, but PDF operators
					 * all move the point as if outputting L2R. We can either reverse the
					 * order of chars (bad, because of cut/paste) or we can perform
					 * gymnastics with the position. We opt for the latter. */
					fz_append_printf(ctx, cb->buf, "[");
					for (i = 0; i < len; i++)
					{
						if (i == 0)
						{
							if (len > 1)
								fz_append_printf(ctx, cb->buf, "%d", -500*(len-1));
						}
						else
							fz_append_printf(ctx, cb->buf, "%d", 1000);
						fz_append_printf(ctx, cb->buf, "<%04x>", word->chars[i]);
					}
					fz_append_printf(ctx, cb->buf, "]TJ\n");
				}
				else
				{
					/* L2R (or mixed) text */
					x = word->bbox[0];
					y = cb->line_bbox[1];
					fz_append_printf(ctx, cb->buf, "%g %g Td\n", x-cb->tx, y-cb->ty);
					cb->tx = x;
					cb->ty = y;

					fz_append_printf(ctx, cb->buf, "<");
					for (i = 0; i < len; i++)
						fz_append_printf(ctx, cb->buf, "%04x", word->chars[i]);
					fz_append_printf(ctx, cb->buf, ">Tj\n");
				}
			}
		}

		cb->line = word->next;
		fz_free(ctx, word);
	}

	cb->line_tail = &cb->line;
	cb->line = NULL;
	cb->line_dirn = 0;
}

static void
queue_word(fz_context *ctx, char_callback_data_t *cb)
{
	word_t *word;
	int line_is_v, line_is_h, word_is_v, word_is_h;

	if (cb->word_len == 0)
		return;

	word = fz_malloc(ctx, sizeof(*word) + (cb->word_len-1)*sizeof(int));
	word->next = NULL;
	word->len = cb->word_len;
	memcpy(word->bbox, cb->word_bbox, 4*sizeof(float));
	memcpy(word->chars, cb->word_chars, cb->word_len * sizeof(int));
	cb->word_len = 0;

	line_is_v = !!(cb->line_dirn & (WORD_CONTAINS_B2T | WORD_CONTAINS_T2B));
	word_is_v = !!(cb->word_dirn & (WORD_CONTAINS_B2T | WORD_CONTAINS_T2B));
	line_is_h = !!(cb->line_dirn & (WORD_CONTAINS_L2R | WORD_CONTAINS_R2L));
	word_is_h = !!(cb->word_dirn & (WORD_CONTAINS_L2R | WORD_CONTAINS_R2L));

	word->dirn = cb->word_dirn;
	cb->word_dirn = 0;

	/* Can we put the new word onto the end of the existing line? */
	if (cb->line != NULL &&
		!line_is_v && !word_is_v &&
		word->bbox[1] <= cb->line_bbox[3] &&
		word->bbox[3] >= cb->line_bbox[1] &&
		(word->bbox[0] >= cb->line_bbox[2] || word->bbox[2] <= cb->line_bbox[0]))
	{
		/* Can append (horizontal motion). */
		if (word->bbox[0] < cb->line_bbox[0])
			cb->line_bbox[0] = word->bbox[0];
		if (word->bbox[1] < cb->line_bbox[1])
			cb->line_bbox[1] = word->bbox[1];
		if (word->bbox[2] > cb->line_bbox[2])
			cb->line_bbox[2] = word->bbox[2];
		if (word->bbox[3] > cb->line_bbox[3])
			cb->line_bbox[3] = word->bbox[3];
	}
	else if (cb->line != NULL &&
		!line_is_h && !word_is_h &&
		word->bbox[0] <= cb->line_bbox[2] &&
		word->bbox[2] >= cb->line_bbox[0] &&
		(word->bbox[1] >= cb->line_bbox[3] || word->bbox[3] <= cb->line_bbox[1]))
	{
		/* Can append (vertical motion). */
		if (!word_is_v)
			word->dirn |= WORD_CONTAINS_T2B;
		if (word->bbox[0] < cb->line_bbox[0])
			cb->line_bbox[0] = word->bbox[0];
		if (word->bbox[1] < cb->line_bbox[1])
			cb->line_bbox[1] = word->bbox[1];
		if (word->bbox[2] > cb->line_bbox[2])
			cb->line_bbox[2] = word->bbox[2];
		if (word->bbox[3] > cb->line_bbox[3])
			cb->line_bbox[3] = word->bbox[3];
	}
	else
	{
		fz_try(ctx)
			flush_words(ctx, cb);
		fz_catch(ctx)
		{
			fz_free(ctx, word);
			fz_rethrow(ctx);
		}
		memcpy(cb->line_bbox, word->bbox, 4*sizeof(float));
	}

	*cb->line_tail = word;
	cb->line_tail = &word->next;
	cb->line_dirn |= word->dirn;
}

static void
char_callback(fz_context *ctx, void *arg, int unicode,
		const char *font_name,
		const int *line_bbox, const int *word_bbox,
		const int *char_bbox, int pointsize)
{
	char_callback_data_t *cb = (char_callback_data_t *)arg;
	pdfocr_band_writer *writer = cb->writer;
	float bbox[4];

	bbox[0] = word_bbox[0] * 72.0f / cb->writer->ocrbitmap->xres;
	bbox[3] = (writer->ocrbitmap->h - 1 - word_bbox[1]) * 72.0f / cb->writer->ocrbitmap->yres;
	bbox[2] = word_bbox[2] * 72.0f / cb->writer->ocrbitmap->yres;
	bbox[1] = (writer->ocrbitmap->h - 1 - word_bbox[3]) * 72.0f / cb->writer->ocrbitmap->yres;

	if (bbox[0] != cb->word_bbox[0] ||
		bbox[1] != cb->word_bbox[1] ||
		bbox[2] != cb->word_bbox[2] ||
		bbox[3] != cb->word_bbox[3])
	{
		queue_word(ctx, cb);
		memcpy(cb->word_bbox, bbox, 4 * sizeof(float));
	}

	if (cb->word_len == 0)
	{
		cb->word_dirn = 0;
		memcpy(cb->word_prev_char_bbox, char_bbox, 4 * sizeof(int));
	}
	else
	{
		int ox = cb->word_prev_char_bbox[0] + cb->word_prev_char_bbox[2];
		int oy = cb->word_prev_char_bbox[1] + cb->word_prev_char_bbox[3];
		int x = char_bbox[0] + char_bbox[2] - ox;
		int y = char_bbox[1] + char_bbox[3] - oy;
		int ax = x < 0 ? -x : x;
		int ay = y < 0 ? -y : y;
		if (ax > ay)
		{
			if (x > 0)
				cb->word_dirn |= WORD_CONTAINS_L2R;
			else if (x < 0)
				cb->word_dirn |= WORD_CONTAINS_R2L;
		}
		else if (ay < ax)
		{
			if (y > 0)
				cb->word_dirn |= WORD_CONTAINS_T2B;
			else if (y < 0)
				cb->word_dirn |= WORD_CONTAINS_B2T;
		}
	}

	if (cb->word_max == cb->word_len)
	{
		int newmax = cb->word_max * 2;
		if (newmax == 0)
			newmax = 16;
		cb->word_chars = fz_realloc_array(ctx, cb->word_chars, newmax, int);
		cb->word_max = newmax;
	}

	cb->word_chars[cb->word_len++] = unicode;
}

static int
pdfocr_progress(fz_context *ctx, void *arg, int prog)
{
	char_callback_data_t *cb = (char_callback_data_t *)arg;
	pdfocr_band_writer *writer = cb->writer;

	if (writer->progress == NULL)
		return 0;

	return writer->progress(ctx, writer->progress_arg, writer->pages - 1, prog);
}

static void
pdfocr_write_trailer(fz_context *ctx, fz_band_writer *writer_)
{
	pdfocr_band_writer *writer = (pdfocr_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int w = writer->super.w;
	int h = writer->super.h;
	int xres = writer->super.xres;
	int yres = writer->super.yres;
	int sh = writer->options.strip_height;
	int strips;
	int i;
	size_t len;
	unsigned char *data;
	fz_buffer *buf = NULL;
	char_callback_data_t cb = { NULL };

	if (sh == 0)
		sh = h;
	strips = (h + sh-1)/sh;

	/* Send the Page contents */
	/* We need the length to this, so write to a buffer first */
	fz_var(buf);
	fz_var(cb);
	fz_try(ctx)
	{
		cb.writer = writer;
		cb.buf = buf = fz_new_buffer(ctx, 0);
		cb.line_tail = &cb.line;
		cb.word_dirn = 0;
		cb.line_dirn = 0;
		fz_append_printf(ctx, buf, "q\n%g 0 0 %g 0 0 cm\n", 72.0f/xres, 72.0f/yres);
		for (i = 0; i < strips; i++)
		{
			int at = h - (i+1)*sh;
			int this_sh = sh;
			if (at < 0)
			{
				this_sh += at;
				at = 0;
			}
			fz_append_printf(ctx, buf, "/P <</MCID 0>> BDC\nq\n%d 0 0 %d 0 %d cm\n/I%d Do\nQ\n",
				w, this_sh, at, i);
		}

		fz_append_printf(ctx, buf, "Q\nBT\n3 Tr\n");

		ocr_recognise(ctx, writer->tessapi, writer->ocrbitmap, char_callback, pdfocr_progress, &cb);
		queue_word(ctx, &cb);
		flush_words(ctx, &cb);
		fz_append_printf(ctx, buf, "ET\n");

		len = fz_buffer_storage(ctx, buf, &data);
		fz_write_printf(ctx, out, "%d 0 obj\n<</Length %zd>>\nstream\n", new_obj(ctx, writer), len);
		fz_write_data(ctx, out, data, len);
		fz_drop_buffer(ctx, buf);
		buf = NULL;
		fz_write_string(ctx, out, "\nendstream\nendobj\n");
	}
	fz_always(ctx)
	{
		fz_free(ctx, cb.word_chars);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_rethrow(ctx);
	}
}

static void
pdfocr_close_band_writer(fz_context *ctx, fz_band_writer *writer_)
{
	pdfocr_band_writer *writer = (pdfocr_band_writer *)writer_;
	fz_output *out = writer->super.out;
	int i;

	/* We actually do the trailer writing in the close */
	if (writer->xref_max > 2)
	{
		int64_t t_pos;

		/* Catalog */
		writer->xref[1] = fz_tell_output(ctx, out);
		fz_write_printf(ctx, out, "1 0 obj\n<</Type/Catalog/Pages 2 0 R>>\nendobj\n");

		/* Page table */
		writer->xref[2] = fz_tell_output(ctx, out);
		fz_write_printf(ctx, out, "2 0 obj\n<</Count %d/Kids[", writer->pages);

		for (i = 0; i < writer->pages; i++)
		{
			if (i > 0)
				fz_write_byte(ctx, out, ' ');
			fz_write_printf(ctx, out, "%d 0 R", writer->page_obj[i]);
		}
		fz_write_string(ctx, out, "]/Type/Pages>>\nendobj\n");

		/* Xref */
		t_pos = fz_tell_output(ctx, out);
		fz_write_printf(ctx, out, "xref\n0 %d\n0000000000 65535 f \n", writer->obj_num);
		for (i = 1; i < writer->obj_num; i++)
			fz_write_printf(ctx, out, "%010ld 00000 n \n", writer->xref[i]);
		fz_write_printf(ctx, out, "trailer\n<</Size %d/Root 1 0 R>>\nstartxref\n%ld\n%%%%EOF\n", writer->obj_num, t_pos);
	}
}

static void
pdfocr_drop_band_writer(fz_context *ctx, fz_band_writer *writer_)
{
	pdfocr_band_writer *writer = (pdfocr_band_writer *)writer_;
	fz_free(ctx, writer->stripbuf);
	fz_free(ctx, writer->compbuf);
	fz_free(ctx, writer->page_obj);
	fz_free(ctx, writer->xref);
	fz_drop_pixmap(ctx, writer->ocrbitmap);
	ocr_fin(ctx, writer->tessapi);
}
#endif

fz_band_writer *fz_new_pdfocr_band_writer(fz_context *ctx, fz_output *out, const fz_pdfocr_options *options)
{
#ifdef OCR_DISABLED
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OCR support in this build");
#else
	pdfocr_band_writer *writer = fz_new_band_writer(ctx, pdfocr_band_writer, out);

	writer->super.header = pdfocr_write_header;
	writer->super.band = pdfocr_write_band;
	writer->super.trailer = pdfocr_write_trailer;
	writer->super.close = pdfocr_close_band_writer;
	writer->super.drop = pdfocr_drop_band_writer;

	if (options)
		writer->options = *options;
	else
		memset(&writer->options, 0, sizeof(writer->options));

	/* Objects:
	 *  1 reserved for catalog
	 *  2 for pages tree
	 *  3 font
	 *  4 cidfont
	 *  5 cid to gid map
	 *  6 tounicode
	 *  7 font descriptor
	 *  8 font file
	 */
	writer->obj_num = 9;

	fz_try(ctx)
	{
		writer->tessapi = ocr_init(ctx, writer->options.language, writer->options.datadir);
	}
	fz_catch(ctx)
	{
		fz_drop_band_writer(ctx, &writer->super);
		fz_throw(ctx, FZ_ERROR_GENERIC, "OCR initialisation failed");
	}

	return &writer->super;
#endif
}

void
fz_pdfocr_band_writer_set_progress(fz_context *ctx, fz_band_writer *writer_, fz_pdfocr_progress_fn *progress, void *progress_arg)
{
#ifdef OCR_DISABLED
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OCR support in this build");
#else
	pdfocr_band_writer *writer = (pdfocr_band_writer *)writer_;
	if (writer == NULL)
		return;
	if (writer->super.header != pdfocr_write_header)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Not a pdfocr band writer!");

	writer->progress = progress;
	writer->progress_arg = progress_arg;
#endif
}

void
fz_save_pixmap_as_pdfocr(fz_context *ctx, fz_pixmap *pixmap, char *filename, int append, const fz_pdfocr_options *pdfocr)
{
#ifdef OCR_DISABLED
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OCR support in this build");
#else
	fz_output *out = fz_new_output_with_path(ctx, filename, append);
	fz_try(ctx)
	{
		fz_write_pixmap_as_pdfocr(ctx, out, pixmap, pdfocr);
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_rethrow(ctx);
#endif
}

/* High-level document writer interface */

#ifndef OCR_DISABLED
typedef struct
{
	fz_document_writer super;
	fz_draw_options draw;
	fz_pdfocr_options pdfocr;
	fz_pixmap *pixmap;
	fz_band_writer *bander;
	fz_output *out;
	int pagenum;
} fz_pdfocr_writer;

static fz_device *
pdfocr_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_pdfocr_writer *wri = (fz_pdfocr_writer*)wri_;
	return fz_new_draw_device_with_options(ctx, &wri->draw, mediabox, &wri->pixmap);
}

static void
pdfocr_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_pdfocr_writer *wri = (fz_pdfocr_writer*)wri_;
	fz_pixmap *pix = wri->pixmap;

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		fz_write_header(ctx, wri->bander, pix->w, pix->h, pix->n, pix->alpha, pix->xres, pix->yres, wri->pagenum++, pix->colorspace, pix->seps);
		fz_write_band(ctx, wri->bander, pix->stride, pix->h, pix->samples);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_pixmap(ctx, pix);
		wri->pixmap = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdfocr_close_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_pdfocr_writer *wri = (fz_pdfocr_writer*)wri_;

	fz_close_band_writer(ctx, wri->bander);
	fz_close_output(ctx, wri->out);
}

static void
pdfocr_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_pdfocr_writer *wri = (fz_pdfocr_writer*)wri_;

	fz_drop_pixmap(ctx, wri->pixmap);
	fz_drop_band_writer(ctx, wri->bander);
	fz_drop_output(ctx, wri->out);
}
#endif

fz_document_writer *
fz_new_pdfocr_writer_with_output(fz_context *ctx, fz_output *out, const char *options)
{
#ifdef OCR_DISABLED
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OCR support in this build");
#else
	fz_pdfocr_writer *wri = NULL;

	fz_var(wri);

	fz_try(ctx)
	{
		wri = fz_new_derived_document_writer(ctx, fz_pdfocr_writer, pdfocr_begin_page, pdfocr_end_page, pdfocr_close_writer, pdfocr_drop_writer);
		fz_parse_draw_options(ctx, &wri->draw, options);
		fz_parse_pdfocr_options(ctx, &wri->pdfocr, options);
		wri->out = out;
		wri->bander = fz_new_pdfocr_band_writer(ctx, wri->out, &wri->pdfocr);
	}
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	return (fz_document_writer*)wri;
#endif
}

fz_document_writer *
fz_new_pdfocr_writer(fz_context *ctx, const char *path, const char *options)
{
#ifdef OCR_DISABLED
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OCR support in this build");
#else
	fz_output *out = fz_new_output_with_path(ctx, path ? path : "out.pdfocr", 0);
	return fz_new_pdfocr_writer_with_output(ctx, out, options);
#endif
}

void
fz_pdfocr_writer_set_progress(fz_context *ctx, fz_document_writer *writer, fz_pdfocr_progress_fn *progress, void *progress_arg)
{
#ifdef OCR_DISABLED
	fz_throw(ctx, FZ_ERROR_GENERIC, "No OCR support in this build");
#else
	fz_pdfocr_writer *wri = (fz_pdfocr_writer *)writer;
	if (!writer)
		return;
	if (writer->begin_page != pdfocr_begin_page)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Not a pdfocr writer!");
	fz_pdfocr_band_writer_set_progress(ctx, wri->bander, progress, progress_arg);
#endif
}
