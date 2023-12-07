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

#undef DEBUG_OCR

#ifndef OCR_DISABLED
#include "tessocr.h"

/*

This device can be used in 2 modes, with or without a list.

In both modes the OCR device is created with a target device. The
caller runs the page to the device, and the device processes the calls
and (eventually) calls through to the target.

In both modes, all incoming calls are forwarded to an internal draw
device to render the page, so the page rendering is always complete.
The incoming calls are also forwarded (mostly, eventually) to the
target. Where the 2 modes differ is in the timing/content of those
forwarded calls.

In the first mode (without a list), the device instantly forwards all
non-text calls to the target. When the OCR device is closed, an OCR pass
is performed, and the recovered text is forwarded to the target. All
recovered text is listed as Courier, and ends up on top of the content.

This is fine for text extraction and probably for most cases of document
conversion. It's no good for correcting the unicode values within a
document though.

So, we have concocted a second way of working, using a display list. In
this mode, as well as rendering every device call that comes in, it
forwards them to a display list (and not the target). When the device
is closed we OCR the text image, and store the results. We then play
the list back through a 'rewrite' device to the target. The rewrite
device rewrites the text objects with the correct unicode values. Any
characters given by the OCR pass that aren't used by the rewrite step
are then sent through as invisible text.

This means that all the target device sees is the exact same graphical
objects in the exact same order, but with corrected unicode values.
Also, any text that appears in the document as a result of images or
line art is sent through as 'invisible' text at the end, so it will work
for cut/paste or search.

Or, at least, that was the plan. Unfortunately, it turns out that
Tesseract (with the LSTM engine (the most modern one)) is really bad at
giving bounding boxes for characters. It seems that the neural network
can say "hey, there is an 'X'", but it can't actually say where the X
occurred within the word. So tesseract knows where the words are, and
knows the order of the letters within the word, but basically guesses
at bboxes for the letters.

Because of this, we can't rely on character bboxes from tesseract to be
correct. We have to work off the word bboxes alone, together with the
order in which characters are passed to us.

So, as Tesseract gives us data, we store the word bbox, together with
the list of chars within that word.

When we play the list back through the display device, we then have to
rewrite text objects based on which word they are in. For the first
version, we'll make the extremely dodgy assumption that characters
come in the same order within the word.

For future versions we may want to collect bboxes for each text char
on our initial list building pass, collate those into matching 'words'
and sort them accordingly.
*/


typedef struct word_record_s {
	int len;
	fz_rect bbox;
	int n;
	int unicode[1];
} word_record;

typedef struct fz_ocr_device_s
{
	fz_device super;

	/* Progress monitoring */
	int (*progress)(fz_context *, void *, int progress);
	void *progress_arg;

	fz_device *target;
	fz_display_list *list;
	fz_device *list_dev;
	fz_device *draw_dev;
	fz_pixmap *pixmap;

	fz_rect mediabox;
	fz_matrix ctm;

	fz_rect word_bbox;
	fz_font *font;

	/* Current word */
	int char_max;
	int char_len;
	int *chars;

	/* Entire page */
	int words_max;
	int words_len;
	word_record **words;

	char *language;
	char *datadir;
} fz_ocr_device;

static void
fz_ocr_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_fill_path(ctx, ocr->list_dev, path, even_odd, ctm, colorspace, color, alpha, color_params);
	fz_fill_path(ctx, ocr->draw_dev, path, even_odd, ctm, colorspace, color, alpha, color_params);
}

static void
fz_ocr_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke,
	fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_stroke_path(ctx, ocr->list_dev, path, stroke, ctm, colorspace, color, alpha, color_params);
	fz_stroke_path(ctx, ocr->draw_dev, path, stroke, ctm, colorspace, color, alpha, color_params);
}

static void
fz_ocr_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	if (ocr->list_dev != ocr->target)
		fz_fill_text(ctx, ocr->list_dev, text, ctm, colorspace, color, alpha, color_params);
	fz_fill_text(ctx, ocr->draw_dev, text, ctm, colorspace, color, alpha, color_params);
}

static void
fz_ocr_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke,
	fz_matrix ctm, fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	if (ocr->list_dev != ocr->target)
		fz_stroke_text(ctx, ocr->list_dev, text, stroke, ctm, colorspace, color, alpha, color_params);
	fz_stroke_text(ctx, ocr->draw_dev, text, stroke, ctm, colorspace, color, alpha, color_params);
}

static void
fz_ocr_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_fill_shade(ctx, ocr->list_dev, shade, ctm, alpha, color_params);
	fz_fill_shade(ctx, ocr->draw_dev, shade, ctm, alpha, color_params);
}

static void
fz_ocr_fill_image(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_fill_image(ctx, ocr->list_dev, image, ctm, alpha, color_params);
	fz_fill_image(ctx, ocr->draw_dev, image, ctm, alpha, color_params);
}

static void
fz_ocr_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_fill_image_mask(ctx, ocr->list_dev, image, ctm, colorspace, color, alpha, color_params);
	fz_fill_image_mask(ctx, ocr->draw_dev, image, ctm, colorspace, color, alpha, color_params);
}

static void
fz_ocr_clip_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_rect scissor)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_clip_path(ctx, ocr->list_dev, path, even_odd, ctm, scissor);
	fz_clip_path(ctx, ocr->draw_dev, path, even_odd, ctm, scissor);
}

static void
fz_ocr_clip_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_clip_stroke_path(ctx, ocr->list_dev, path, stroke, ctm, scissor);
	fz_clip_stroke_path(ctx, ocr->draw_dev, path, stroke, ctm, scissor);
}

static void
fz_ocr_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	if (ocr->list_dev != ocr->target)
		fz_clip_text(ctx, ocr->list_dev, text, ctm, scissor);
	fz_clip_text(ctx, ocr->draw_dev, text, ctm, scissor);
}

static void
fz_ocr_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	if (ocr->list_dev != ocr->target)
		fz_clip_stroke_text(ctx, ocr->list_dev, text, stroke, ctm, scissor);
	fz_clip_stroke_text(ctx, ocr->draw_dev, text, stroke, ctm, scissor);
}

static void
fz_ocr_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	/* Ignore text is generally used when text has been sent as
	 * part of other graphics - such as line art or images. As such
	 * we'll pick up the 'true' unicode values of such text in the
	 * OCR phase. We therefore send text to the list device (so
	 * it can be rewritten), but not direct to the target. */
	if (ocr->list_dev != ocr->target)
		fz_ignore_text(ctx, ocr->list_dev, text, ctm);
	fz_ignore_text(ctx, ocr->draw_dev, text, ctm);
}

static void
fz_ocr_clip_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, fz_rect scissor)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_clip_image_mask(ctx, ocr->list_dev, image, ctm, scissor);
	fz_clip_image_mask(ctx, ocr->draw_dev, image, ctm, scissor);
}

static void
fz_ocr_pop_clip(fz_context *ctx, fz_device *dev)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_pop_clip(ctx, ocr->list_dev);
	fz_pop_clip(ctx, ocr->draw_dev);
}

static void
fz_ocr_begin_mask(fz_context *ctx, fz_device *dev, fz_rect rect, int luminosity, fz_colorspace *colorspace, const float *color, fz_color_params color_params)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_begin_mask(ctx, ocr->list_dev, rect, luminosity, colorspace, color, color_params);
	fz_begin_mask(ctx, ocr->draw_dev, rect, luminosity, colorspace, color, color_params);
}

static void
fz_ocr_end_mask(fz_context *ctx, fz_device *dev)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_end_mask(ctx, ocr->list_dev);
	fz_end_mask(ctx, ocr->draw_dev);
}

static void
fz_ocr_begin_group(fz_context *ctx, fz_device *dev, fz_rect rect, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_begin_group(ctx, ocr->list_dev, rect, cs, isolated, knockout, blendmode, alpha);
	fz_begin_group(ctx, ocr->draw_dev, rect, cs, isolated, knockout, blendmode, alpha);
}

static void
fz_ocr_end_group(fz_context *ctx, fz_device *dev)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_end_group(ctx, ocr->list_dev);
	fz_end_group(ctx, ocr->draw_dev);
}

static int
fz_ocr_begin_tile(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	/* Always pass 0 as tile id here so that neither device can
	 * disagree about whether the contents need to be sent. */
	(void)fz_begin_tile_id(ctx, ocr->list_dev, area, view, xstep, ystep, ctm, 0);
	(void)fz_begin_tile_id(ctx, ocr->draw_dev, area, view, xstep, ystep, ctm, 0);

	return 0;
}

static void
fz_ocr_end_tile(fz_context *ctx, fz_device *dev)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_end_tile(ctx, ocr->list_dev);
	fz_end_tile(ctx, ocr->draw_dev);
}

static void
fz_ocr_render_flags(fz_context *ctx, fz_device *dev, int set, int clear)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_render_flags(ctx, ocr->list_dev, set, clear);
	fz_render_flags(ctx, ocr->draw_dev, set, clear);
}

static void
fz_ocr_set_default_colorspaces(fz_context *ctx, fz_device *dev, fz_default_colorspaces *cs)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_set_default_colorspaces(ctx, ocr->list_dev, cs);
	fz_set_default_colorspaces(ctx, ocr->draw_dev, cs);
}

static void
fz_ocr_begin_layer(fz_context *ctx, fz_device *dev, const char *layer_name)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_begin_layer(ctx, ocr->list_dev, layer_name);
	fz_begin_layer(ctx, ocr->draw_dev, layer_name);
}

static void
fz_ocr_end_layer(fz_context *ctx, fz_device *dev)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;

	fz_end_layer(ctx, ocr->list_dev);
	fz_end_layer(ctx, ocr->draw_dev);
}

static void
drop_ocr_device(fz_context *ctx, fz_ocr_device *ocr)
{
	int i;

	if (ocr == NULL)
		return;

	if (ocr->list_dev != ocr->target)
		fz_drop_device(ctx, ocr->list_dev);
	fz_drop_display_list(ctx, ocr->list);
	fz_drop_device(ctx, ocr->draw_dev);
	fz_drop_pixmap(ctx, ocr->pixmap);
	for (i = 0; i < ocr->words_len; i++)
		fz_free(ctx, ocr->words[i]);
	fz_free(ctx, ocr->words);
	fz_free(ctx, ocr->chars);
	fz_free(ctx, ocr->language);
	fz_free(ctx, ocr->datadir);
}

static void
flush_word(fz_context *ctx, fz_ocr_device *ocr)
{
	float color = 1;
	fz_color_params params = { 0 };
	int i;
	fz_text *text = NULL;
	fz_matrix trm;
	float step;
	fz_rect char_bbox;

	if (ocr->char_len == 0)
		return;

	/* If we're not sending direct to the target device, then insert
	 * all the chars we've found into a table so we can rewrite
	 * the text objects that come from the list device on the fly.
	 */
	if (ocr->list_dev != ocr->target)
	{
		word_record *word;

		if (ocr->words_len == ocr->words_max)
		{
			int new_max = ocr->words_max * 2;
			if (new_max == 0)
				new_max = 32;
			ocr->words = fz_realloc_array(ctx, ocr->words, new_max, word_record *);
			ocr->words_max = new_max;
		}
		word = (word_record *)Memento_label(fz_malloc(ctx, sizeof(word_record) + sizeof(int) * (ocr->char_len-1)), "word_record");
		word->len = ocr->char_len;
		word->bbox = ocr->word_bbox;
		word->n = 0;
		memcpy(word->unicode, ocr->chars, ocr->char_len * sizeof(int));
		ocr->words[ocr->words_len++] = word;
		ocr->char_len = 0;
		return;
	}
	/* FIXME: Look at font-name. */
	/* All this is a bit horrid, because the detection of sizes for
	 * the glyphs depends on the width of the glyphs. Use Courier
	 * because it's monospaced. */
	if (ocr->font == NULL)
		ocr->font = fz_new_base14_font(ctx, "Courier");

	fz_var(text);

	fz_try(ctx)
	{
		text = fz_new_text(ctx);

		/* Divide the word box into equal lengths. */
		/* This falls down when we have words with chars of
		 * different widths in, but it's acceptable for these
		 * purposes. */
		/* FIXME: This assumes L2R motion of text. */
		step = (ocr->word_bbox.x1 - ocr->word_bbox.x0) / ocr->char_len;
		char_bbox.x1 = ocr->word_bbox.x0;
		char_bbox.y0 = ocr->word_bbox.y0;
		char_bbox.y1 = ocr->word_bbox.y1;
		for (i = 0; i < ocr->char_len; i++)
		{
			char_bbox.x0 = char_bbox.x1;
			char_bbox.x1 += step;
			/* Horrid constants that happen to work with Courier. */
			trm.a = 10.0f/6 * (char_bbox.x1 - char_bbox.x0);
			trm.b = 0;
			trm.c = 0;
			trm.d = 10.0f/6 * (char_bbox.y1 - char_bbox.y0);
			trm.e = char_bbox.x0;
			trm.f = char_bbox.y0;
			fz_show_glyph(ctx, text, ocr->font, trm,
				fz_encode_character(ctx, ocr->font, ocr->chars[i]), ocr->chars[i],
					0, 0, FZ_BIDI_LTR, 0);
		}

		fz_fill_text(ctx, ocr->target, text, fz_identity,
				fz_device_gray(ctx), &color, 1, params);
	}
	fz_always(ctx)
	{
		fz_drop_text(ctx, text);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	ocr->char_len = 0;
}

static void
char_callback(fz_context *ctx, void *arg, int unicode,
		const char *font_name,
		const int *line_bbox, const int *word_bbox,
		const int *char_bbox, int pointsize)
{
	fz_ocr_device *ocr = (fz_ocr_device *)arg;
	fz_rect bbox = { word_bbox[0]-1, word_bbox[1]-1, word_bbox[2]+1, word_bbox[3]+1 };

	if (bbox.x0 != ocr->word_bbox.x0 ||
		bbox.y0 != ocr->word_bbox.y0 ||
		bbox.x1 != ocr->word_bbox.x1 ||
		bbox.y1 != ocr->word_bbox.y1)
	{
		flush_word(ctx, ocr);
		ocr->word_bbox = bbox;
	}

	if (ocr->char_max == ocr->char_len)
	{
		int new_max = ocr->char_max * 2;
		if (new_max == 0)
			new_max = 32;
		ocr->chars = fz_realloc_array(ctx, ocr->chars, new_max, int);
		ocr->char_max = new_max;
	}

	ocr->chars[ocr->char_len++] = unicode;
}


typedef struct
{
	fz_device super;

	fz_device *target;
	int words_len;
	word_record **words;
	int current;
} fz_rewrite_device;

static fz_text_span *
fz_clone_text_span(fz_context *ctx, const fz_text_span *span)
{
	fz_text_span *cspan;

	if (span == NULL)
		return NULL;

	cspan = fz_malloc_struct(ctx, fz_text_span);
	*cspan = *span;
	cspan->cap = cspan->len;
	cspan->items = fz_calloc_no_throw(ctx, cspan->len, sizeof(*cspan->items));
	if (cspan->items == NULL)
	{
		fz_free(ctx, cspan);
		fz_throw(ctx, FZ_ERROR_SYSTEM, "calloc (%zu x %zu bytes) failed", (size_t)cspan->len, sizeof(*cspan->items));
	}
	memcpy(cspan->items, span->items, sizeof(*cspan->items) * cspan->len);
	fz_keep_font(ctx, cspan->font);

	return cspan;
}

#ifdef DEBUG_OCR
static void
debug_word(fz_context *ctx, word_record *word)
{
	int i;

	fz_write_printf(ctx, fz_stdout(ctx), "   %g %g %g %g:",
			word->bbox.x0,
			word->bbox.y0,
			word->bbox.x1,
			word->bbox.y1);

	for (i = 0; i < word->n; i++)
	{
		int unicode = word->unicode[i];
		if (unicode >= 32 && unicode < 127)
			fz_write_printf(ctx, fz_stdout(ctx), "%c", unicode);
		else
			fz_write_printf(ctx, fz_stdout(ctx), "<%04x>", unicode);
	}
	if (word->n < word->len)
	{
		int unicode = word->unicode[i++];
		if (unicode >= 32 && unicode < 127)
			fz_write_printf(ctx, fz_stdout(ctx), "{%c}", unicode);
		else
			fz_write_printf(ctx, fz_stdout(ctx), "{<%04x>}", unicode);
		for (; i < word->len; i++)
		{
			int unicode = word->unicode[i];
			if (unicode >= 32 && unicode < 127)
				fz_write_printf(ctx, fz_stdout(ctx), "%c", unicode);
			else
				fz_write_printf(ctx, fz_stdout(ctx), "<%04x>", unicode);
		}
	}
	fz_write_printf(ctx, fz_stdout(ctx), "\n");
}
#endif

static void
rewrite_char(fz_context *ctx, fz_rewrite_device *dev, fz_matrix ctm, fz_text_item *item, fz_point vadv)
{
	int i, start;
	fz_point p = { item->x, item->y };

	/* No point in trying to rewrite spaces! */
	if (item->ucs == 32)
		return;

	p = fz_transform_point(p, ctm);
	p.x += vadv.x/2;
	p.y += vadv.y/2;

#ifdef DEBUG_OCR
	fz_write_printf(ctx, fz_stdout(ctx), "Looking for '%c' at %g %g\n", item->ucs, p.x, p.y);
#endif

	start = dev->current;
	for (i = start; i < dev->words_len; i++)
	{
#ifdef DEBUG_OCR
		debug_word(ctx, dev->words[i]);
#endif
		if (dev->words[i]->n >= dev->words[i]->len)
			continue;
		if (dev->words[i]->bbox.x0 <= p.x &&
			dev->words[i]->bbox.x1 >= p.x &&
			dev->words[i]->bbox.y0 <= p.y &&
			dev->words[i]->bbox.y1 >= p.y)
		{
			item->ucs = dev->words[i]->unicode[dev->words[i]->n++];
			dev->current = i;
			return;
		}
	}
	for (i = 0; i < start; i++)
	{
#ifdef DEBUG_OCR
		debug_word(ctx, dev->words[i]);
#endif
		if (dev->words[i]->n >= dev->words[i]->len)
			continue;
		if (dev->words[i]->bbox.x0 <= p.x &&
			dev->words[i]->bbox.x1 >= p.x &&
			dev->words[i]->bbox.y0 <= p.y &&
			dev->words[i]->bbox.y1 >= p.y)
		{
			item->ucs = dev->words[i]->unicode[dev->words[i]->n++];
			dev->current = i;
			return;
		}
	}
}

static fz_text_span *
rewrite_span(fz_context *ctx, fz_rewrite_device *dev, fz_matrix ctm, const fz_text_span *span)
{
	fz_text_span *rspan = fz_clone_text_span(ctx, span);
	int wmode = span->wmode;
	int i;
	fz_point dir;
	fz_matrix trm = span->trm;

	trm.e = 0;
	trm.f = 0;
	trm = fz_concat(trm, ctm);

	if (wmode == 0)
	{
		dir.x = 1;
		dir.y = 0;
	}
	else
	{
		dir.x = 0;
		dir.y = -1;
	}
	dir = fz_transform_vector(dir, trm);

	/* And do the actual rewriting */
	for (i = 0; i < rspan->len; i++) {
		float advance = fz_advance_glyph(ctx, span->font, rspan->items[i].gid, wmode);
		fz_point vadv = { dir.x * advance, dir.y * advance };
		rewrite_char(ctx, dev, ctm, &rspan->items[i], vadv);
	}

	return rspan;
}

static fz_text *
rewrite_text(fz_context *ctx, fz_rewrite_device *dev, fz_matrix ctm, const fz_text *text)
{
	fz_text *rtext = fz_new_text(ctx);
	fz_text_span *span = text->head;
	fz_text_span **dspan = &rtext->head;

	fz_try(ctx)
	{
		while (span)
		{
			*dspan = rewrite_span(ctx, dev, ctm, span);
			rtext->tail = *dspan;
			dspan = &(*dspan)->next;
			span = span->next;
		}
	}
	fz_catch(ctx)
	{
		fz_drop_text(ctx, rtext);
		fz_rethrow(ctx);
	}

	return rtext;
}

static void
rewrite_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params params)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_fill_path(ctx, rewrite->target, path, even_odd, ctm, cs, color, alpha, params);
}

static void
rewrite_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params params)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_stroke_path(ctx, rewrite->target, path, stroke, ctm, cs, color, alpha, params);
}

static void
rewrite_clip_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_rect scissor)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_clip_path(ctx, rewrite->target, path, even_odd, ctm, scissor);
}

static void
rewrite_clip_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_clip_stroke_path(ctx, rewrite->target, path, stroke, ctm, scissor);
}

static void
rewrite_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params params)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;
	fz_text *rtext = rewrite_text(ctx, rewrite, ctm, text);

	fz_try(ctx)
		fz_fill_text(ctx, rewrite->target, rtext, ctm, cs, color, alpha, params);
	fz_always(ctx)
		fz_drop_text(ctx, rtext);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
rewrite_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params params)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;
	fz_text *rtext = rewrite_text(ctx, rewrite, ctm, text);

	fz_try(ctx)
		fz_stroke_text(ctx, rewrite->target, rtext, stroke, ctm, cs, color, alpha, params);
	fz_always(ctx)
		fz_drop_text(ctx, rtext);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
rewrite_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;
	fz_text *rtext = rewrite_text(ctx, rewrite, ctm, text);

	fz_try(ctx)
		fz_clip_text(ctx, rewrite->target, rtext, ctm, scissor);
	fz_always(ctx)
		fz_drop_text(ctx, rtext);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
rewrite_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;
	fz_text *rtext = rewrite_text(ctx, rewrite, ctm, text);

	fz_try(ctx)
		fz_clip_stroke_text(ctx, rewrite->target, rtext, stroke, ctm, scissor);
	fz_always(ctx)
		fz_drop_text(ctx, rtext);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
rewrite_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;
	fz_text *rtext = rewrite_text(ctx, rewrite, ctm, text);

	fz_try(ctx)
		fz_ignore_text(ctx, rewrite->target, rtext, ctm);
	fz_always(ctx)
		fz_drop_text(ctx, rtext);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
rewrite_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shd, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_fill_shade(ctx, rewrite->target, shd, ctm, alpha, color_params);
}

static void
rewrite_fill_image(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_fill_image(ctx, rewrite->target, img, ctm, alpha, color_params);
}

static void
rewrite_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm, fz_colorspace *cs, const float *color, float alpha, fz_color_params color_params)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_fill_image_mask(ctx, rewrite->target, img, ctm, cs, color, alpha, color_params);
}

static void
rewrite_clip_image_mask(fz_context *ctx, fz_device *dev, fz_image *img, fz_matrix ctm, fz_rect scissor)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_clip_image_mask(ctx, rewrite->target, img, ctm, scissor);
}

static void
rewrite_pop_clip(fz_context *ctx, fz_device *dev)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_pop_clip(ctx, rewrite->target);
}

static void
rewrite_begin_mask(fz_context *ctx, fz_device *dev, fz_rect area, int luminosity, fz_colorspace *cs, const float *bc, fz_color_params params)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_begin_mask(ctx, rewrite->target, area, luminosity, cs, bc, params);
}

static void
rewrite_end_mask(fz_context *ctx, fz_device *dev)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_end_mask(ctx, rewrite->target);
}

static void
rewrite_begin_group(fz_context *ctx, fz_device *dev, fz_rect area, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_begin_group(ctx, rewrite->target, area, cs, isolated, knockout, blendmode, alpha);
}

static void
rewrite_end_group(fz_context *ctx, fz_device *dev)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_end_group(ctx, rewrite->target);
}

static int
rewrite_begin_tile(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	return fz_begin_tile_id(ctx, rewrite->target, area, view, xstep, ystep, ctm, id);
}

static void
rewrite_end_tile(fz_context *ctx, fz_device *dev)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_end_tile(ctx, rewrite->target);
}

static void
rewrite_render_flags(fz_context *ctx, fz_device *dev, int set, int clear)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_render_flags(ctx, rewrite->target, set, clear);
}

static void
rewrite_set_default_colorspaces(fz_context *ctx, fz_device *dev, fz_default_colorspaces *cs)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_set_default_colorspaces(ctx, rewrite->target, cs);
}

static void
rewrite_begin_layer(fz_context *ctx, fz_device *dev, const char *layer_name)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_begin_layer(ctx, rewrite->target, layer_name);
}

static void
rewrite_end_layer(fz_context *ctx, fz_device *dev)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;

	fz_end_layer(ctx, rewrite->target);
}

static void
rewrite_close(fz_context *ctx, fz_device *dev)
{
	fz_rewrite_device *rewrite = (fz_rewrite_device *)dev;
	fz_font *font;
	fz_text *text = NULL;
	fz_matrix trm;
	int i, j;

	/* All this is a bit horrid, because the detection of sizes for
	 * the glyphs depends on the width of the glyphs. Use Courier
	 * because it's monospaced. */
	font = fz_new_base14_font(ctx, "Courier");

	fz_var(text);

	fz_try(ctx)
	{
		text = fz_new_text(ctx);

		for (i = 0; i < rewrite->words_len; i++)
		{
			word_record *word = rewrite->words[i];
			fz_rect char_bbox;
			float step;

			if (word->n >= word->len)
				continue;
			step = (word->bbox.x1 - word->bbox.x0) / word->len;
			char_bbox.x1 = word->bbox.x0;
			char_bbox.y0 = word->bbox.y0;
			char_bbox.y1 = word->bbox.y1;
			for (j = 0; j < word->len; j++)
			{
				char_bbox.x0 = char_bbox.x1;
				char_bbox.x1 += step;
				/* Horrid constants that happen to work with Courier. */
				trm.a = 10.0f/6 * (char_bbox.x1 - char_bbox.x0);
				trm.b = 0;
				trm.c = 0;
				trm.d = (char_bbox.y1 - char_bbox.y0);
				trm.e = char_bbox.x0;
				trm.f = char_bbox.y0;
				fz_show_glyph(ctx, text, font, trm,
					word->unicode[j], word->unicode[j],
					0, 0, FZ_BIDI_LTR, 0);
			}
		}

		fz_ignore_text(ctx, rewrite->target, text, fz_identity);
	}
	fz_always(ctx)
	{
		fz_drop_text(ctx, text);
		fz_drop_font(ctx, font);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static fz_device *
new_rewrite_device(fz_context *ctx, fz_device *target, word_record **words, int words_len)
{
	fz_rewrite_device *rewrite;

	rewrite = fz_new_derived_device(ctx, fz_rewrite_device);

	rewrite->super.close_device = rewrite_close;

	rewrite->super.fill_path = rewrite_fill_path;
	rewrite->super.stroke_path = rewrite_stroke_path;
	rewrite->super.clip_path = rewrite_clip_path;
	rewrite->super.clip_stroke_path = rewrite_clip_stroke_path;

	rewrite->super.fill_text = rewrite_fill_text;
	rewrite->super.stroke_text = rewrite_stroke_text;
	rewrite->super.clip_text = rewrite_clip_text;
	rewrite->super.clip_stroke_text = rewrite_clip_stroke_text;
	rewrite->super.ignore_text = rewrite_ignore_text;

	rewrite->super.fill_shade = rewrite_fill_shade;
	rewrite->super.fill_image = rewrite_fill_image;
	rewrite->super.fill_image_mask = rewrite_fill_image_mask;
	rewrite->super.clip_image_mask = rewrite_clip_image_mask;

	rewrite->super.pop_clip = rewrite_pop_clip;

	rewrite->super.begin_mask = rewrite_begin_mask;
	rewrite->super.end_mask = rewrite_end_mask;
	rewrite->super.begin_group = rewrite_begin_group;
	rewrite->super.end_group = rewrite_end_group;

	rewrite->super.begin_tile = rewrite_begin_tile;
	rewrite->super.end_tile = rewrite_end_tile;

	rewrite->super.render_flags = rewrite_render_flags;
	rewrite->super.set_default_colorspaces = rewrite_set_default_colorspaces;

	rewrite->super.begin_layer = rewrite_begin_layer;
	rewrite->super.end_layer = rewrite_end_layer;

	rewrite->target = target;
	rewrite->words = words;
	rewrite->words_len = words_len;
	rewrite->current = 0;

	return &rewrite->super;
}

static int
fz_ocr_progress(fz_context *ctx, void *arg, int prog)
{
	fz_ocr_device *ocr = (fz_ocr_device *)arg;

	if (ocr->progress == NULL)
		return 0;

	return ocr->progress(ctx, ocr->progress_arg, prog);
}

static void
fz_ocr_close_device(fz_context *ctx, fz_device *dev)
{
	fz_ocr_device *ocr = (fz_ocr_device *)dev;
	void *tessapi;
	fz_device *rewrite_device;
	fz_rect bbox;

	fz_close_device(ctx, ocr->draw_dev);

	/* Now run the OCR */
	tessapi = ocr_init(ctx, ocr->language, ocr->datadir);

	fz_try(ctx)
	{
		ocr_recognise(ctx, tessapi, ocr->pixmap, char_callback, &fz_ocr_progress, ocr);
		flush_word(ctx, ocr);
	}
	fz_always(ctx)
		ocr_fin(ctx, tessapi);
	fz_catch(ctx)
		fz_rethrow(ctx);

	/* If we're not using a list, we're done! */
	if (ocr->list_dev == ocr->target)
		return;

	fz_close_device(ctx, ocr->list_dev);

	bbox = fz_transform_rect(ocr->mediabox, ocr->ctm);
	rewrite_device = new_rewrite_device(ctx, ocr->target, ocr->words, ocr->words_len);
	fz_try(ctx)
	{
		fz_run_display_list(ctx, ocr->list, rewrite_device,
					fz_identity, bbox, NULL);
	}
	fz_always(ctx)
	{
		fz_close_device(ctx, rewrite_device);
		fz_drop_device(ctx, rewrite_device);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
fz_ocr_drop_device(fz_context *ctx, fz_device *dev)
{
	drop_ocr_device(ctx, (fz_ocr_device *)dev);
}
#endif

fz_device *
fz_new_ocr_device(fz_context *ctx,
		fz_device *target,
		fz_matrix ctm,
		fz_rect mediabox,
		int with_list,
		const char *language,
		const char *datadir,
		int (*progress)(fz_context *, void *, int),
		void *progress_arg)
{
#ifdef OCR_DISABLED
	fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "OCR Disabled in this build");
#else
	fz_ocr_device *dev;

	if (target == NULL)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "OCR devices require a target");

	dev = fz_new_derived_device(ctx, fz_ocr_device);

	dev->super.close_device = fz_ocr_close_device;
	dev->super.drop_device = fz_ocr_drop_device;

	dev->super.fill_path = fz_ocr_fill_path;
	dev->super.stroke_path = fz_ocr_stroke_path;
	dev->super.clip_path = fz_ocr_clip_path;
	dev->super.clip_stroke_path = fz_ocr_clip_stroke_path;

	dev->super.fill_text = fz_ocr_fill_text;
	dev->super.stroke_text = fz_ocr_stroke_text;
	dev->super.clip_text = fz_ocr_clip_text;
	dev->super.clip_stroke_text = fz_ocr_clip_stroke_text;
	dev->super.ignore_text = fz_ocr_ignore_text;

	dev->super.fill_shade = fz_ocr_fill_shade;
	dev->super.fill_image = fz_ocr_fill_image;
	dev->super.fill_image_mask = fz_ocr_fill_image_mask;
	dev->super.clip_image_mask = fz_ocr_clip_image_mask;

	dev->super.pop_clip = fz_ocr_pop_clip;

	dev->super.begin_mask = fz_ocr_begin_mask;
	dev->super.end_mask = fz_ocr_end_mask;
	dev->super.begin_group = fz_ocr_begin_group;
	dev->super.end_group = fz_ocr_end_group;

	dev->super.begin_tile = fz_ocr_begin_tile;
	dev->super.end_tile = fz_ocr_end_tile;

	dev->super.render_flags = fz_ocr_render_flags;
	dev->super.set_default_colorspaces = fz_ocr_set_default_colorspaces;
	dev->super.begin_layer = fz_ocr_begin_layer;
	dev->super.end_layer = fz_ocr_end_layer;

	dev->progress = progress;
	dev->progress_arg = progress_arg;

	fz_try(ctx)
	{
		fz_rect bbox;
		fz_irect ibox;
		fz_point res;

		dev->target = target;
		dev->mediabox = mediabox;
		dev->ctm = ctm;

		bbox = fz_transform_rect(mediabox, ctm);
		ibox = fz_round_rect(bbox);
		/* Fudge the width to be a multiple of 4. */
		ibox.x1 += (4-(ibox.x1-ibox.x0)) & 3;
		dev->pixmap = fz_new_pixmap_with_bbox(ctx, fz_device_gray(ctx),
							ibox, NULL, 0);
		fz_clear_pixmap(ctx, dev->pixmap);
		res = fz_transform_point_xy(72, 72, ctm);
		if (res.x < 0)
			res.x = -res.x;
		if (res.x < 1)
			res.x = 1;
		if (res.y < 0)
			res.y = -res.y;
		if (res.y < 1)
			res.y = 1;
		fz_set_pixmap_resolution(ctx, dev->pixmap, res.x, res.y);

		dev->language = fz_strdup(ctx, language ? language : "eng");
		dev->datadir = fz_strdup(ctx, datadir ? datadir : "");

		dev->draw_dev = fz_new_draw_device(ctx, fz_identity, dev->pixmap);
		if (with_list)
		{
			dev->list = fz_new_display_list(ctx, mediabox);
			dev->list_dev = fz_new_list_device(ctx, dev->list);
		} else
			dev->list_dev = dev->target;
	}
	fz_catch(ctx)
	{
		drop_ocr_device(ctx, dev);
		fz_rethrow(ctx);
	}

	return (fz_device*)dev;
#endif
}
