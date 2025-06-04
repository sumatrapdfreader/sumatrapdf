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


static int s_xml_starttag_begin(fz_context *ctx, fz_output *out, const char *id)
{
	fz_write_printf(ctx, out, "<%s", id);
	return 0;
}

static int s_xml_starttag_end(fz_context *ctx, fz_output *out)
{
	fz_write_printf(ctx, out, ">\n");
	return 0;
}

static int s_xml_starttag_empty_end(fz_context *ctx, fz_output *out)
{
	fz_write_printf(ctx, out, "/>\n");
	return 0;
}

static int s_xml_endtag(fz_context *ctx, fz_output *out, const char *id)
{
	fz_write_printf(ctx, out, "</%s>\n", id);
	return 0;
}

static int s_write_attribute_int(fz_context *ctx, fz_output *out, const char *id, int value)
{
	fz_write_printf(ctx, out, " %s=\"%i\"", id, value);
	return 0;
}

static int s_write_attribute_size(fz_context *ctx, fz_output *out, const char *id, size_t value)
{
	fz_write_printf(ctx, out, " %s=\"%zi\"", id, value);
	return 0;
}

static int s_write_attribute_float(fz_context *ctx, fz_output *out, const char *id, float value)
{
	fz_write_printf(ctx, out, " %s=\"%g\"", id, value);
	return 0;
}

static int s_write_attribute_string(fz_context *ctx, fz_output *out, const char *id, const char *value)
{
	fz_write_printf(ctx, out, " %s=\"%s\"", id, value);
	return 0;
}

static int s_write_attribute_char(fz_context *ctx, fz_output *out, const char *id, char value)
{
	if (value == '"') fz_write_printf(ctx, out, " %s=\"\\%c\"", id, value);
	else fz_write_printf(ctx, out, " %s=\"%c\"", id, value);
	return 0;
}

static int s_write_attribute_matrix(fz_context *ctx, fz_output *out, const char *id, const fz_matrix *matrix)
{
	fz_write_printf(ctx, out,
		" %s=\"%g %g %g %g %g %g\"",
		id,
		matrix->a,
		matrix->b,
		matrix->c,
		matrix->d,
		matrix->e,
		matrix->f
		);
	return 0;
}




typedef struct
{
	fz_device super;
	fz_output *out;
} fz_xmltext_device;

static void
fz_xmltext_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_xmltext_device *dev = (fz_xmltext_device*) dev_;

	fz_text_span *span;
	for (span = text->head; span; span = span->next)
	{
		int i;

		s_xml_starttag_begin(ctx, dev->out, "span");
		s_write_attribute_matrix(ctx, dev->out, "ctm", &ctm);
		s_write_attribute_string(ctx, dev->out, "font_name", span->font->name);
		if (span->font->flags.is_mono)          s_write_attribute_int(ctx, dev->out, "is_mono", 1);
		if (span->font->flags.is_serif)         s_write_attribute_int(ctx, dev->out, "is_serif", 1);
		if (span->font->flags.is_italic)        s_write_attribute_int(ctx, dev->out, "is_italic", 1);
		if (span->font->flags.ft_substitute)    s_write_attribute_int(ctx, dev->out, "ft_substitute", 1);
		if (span->font->flags.ft_stretch)       s_write_attribute_int(ctx, dev->out, "ft_stretch", 1);
		if (span->font->flags.fake_bold)        s_write_attribute_int(ctx, dev->out, "fake_bold", 1);
		if (span->font->flags.fake_italic)      s_write_attribute_int(ctx, dev->out, "fake_italic", 1);
		if (span->font->flags.has_opentype)     s_write_attribute_int(ctx, dev->out, "has_opentype", 1);
		if (span->font->flags.invalid_bbox)     s_write_attribute_int(ctx, dev->out, "invalid_bbox", 1);
		s_write_attribute_matrix(ctx, dev->out, "trm", &span->trm);
		s_write_attribute_int(ctx, dev->out, "len", span->len);
		s_write_attribute_int(ctx, dev->out, "wmode", span->wmode);
		s_write_attribute_int(ctx, dev->out, "bidi_level", span->bidi_level);
		s_write_attribute_int(ctx, dev->out, "markup_dir", span->markup_dir);
		s_write_attribute_int(ctx, dev->out, "language", span->language);
		s_write_attribute_int(ctx, dev->out, "cap", span->cap);
		s_xml_starttag_end(ctx, dev->out);

		for (i=0; i<span->len; ++i)
		{
			fz_text_item *item = &span->items[i];

			s_xml_starttag_begin(ctx, dev->out, "char");
			s_write_attribute_float(ctx, dev->out, "x", item->x);
			s_write_attribute_float(ctx, dev->out, "y", item->y);
			s_write_attribute_int(ctx, dev->out, "gid", item->gid);
			s_write_attribute_int(ctx, dev->out, "ucs", item->ucs);

			/*
			 * Firefox complains if we put special characters here; it's only for debugging
			 * so this isn't really a problem.
			 */
			s_write_attribute_char(ctx, dev->out, "debug_char",
				(item->ucs >= 32 && item->ucs < 128 && item->ucs != '"')
					? item->ucs : ' '
				);
			s_write_attribute_float(ctx, dev->out, "adv", span->items[i].adv);
			s_xml_starttag_empty_end(ctx, dev->out);
		}

		s_xml_endtag(ctx, dev->out, "span");
	}
}

static void
fz_xmltext_fill_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_xmltext_text(ctx, dev_, text, ctm, colorspace, color, alpha, color_params);
}

static void
fz_xmltext_stroke_text(fz_context *ctx, fz_device *dev_, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	fz_xmltext_text(ctx, dev_, text, ctm, colorspace, color, alpha, color_params);
}

static void
fz_xmltext_clip_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	fz_xmltext_text(ctx, dev_, text, ctm, NULL, NULL, 0 /*alpha*/, fz_default_color_params);
}

static void
fz_xmltext_clip_stroke_text(fz_context *ctx, fz_device *dev_, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	fz_xmltext_text(ctx, dev_, text, ctm, NULL, 0, 0, fz_default_color_params);
}

static void
fz_xmltext_ignore_text(fz_context *ctx, fz_device *dev_, const fz_text *text, fz_matrix ctm)
{
}

static void
fz_stext_close_device(fz_context *ctx, fz_device *dev_)
{
}



static void fz_xmltext_fill_image(fz_context *ctx, fz_device *dev_, fz_image *img, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	fz_xmltext_device *dev = (fz_xmltext_device*) dev_;
	fz_pixmap *pixmap = NULL;
	fz_try(ctx)
	{
		const char *type = NULL;
		fz_compressed_buffer *compressed;
		s_xml_starttag_begin(ctx, dev->out, "image");
		/* First try to write compressed data. */
		compressed = fz_compressed_image_buffer(ctx, img);
		if (compressed)
		{
			if (compressed->params.type == FZ_IMAGE_UNKNOWN)
			{
				/* unknown image type. */
			}
			else if (compressed->params.type == FZ_IMAGE_RAW)
			{
				type = "raw";
				s_write_attribute_string(ctx, dev->out, "type", type);
			}
			else if (compressed->params.type == FZ_IMAGE_FAX)
			{
				type = "fax";
				s_write_attribute_string(ctx, dev->out, "type", type);
				s_write_attribute_int(ctx, dev->out, "columns", compressed->params.u.fax.columns);
				s_write_attribute_int(ctx, dev->out, "rows", compressed->params.u.fax.rows);
				s_write_attribute_int(ctx, dev->out, "k", compressed->params.u.fax.k);
				s_write_attribute_int(ctx, dev->out, "end_of_line", compressed->params.u.fax.end_of_line);
				s_write_attribute_int(ctx, dev->out, "encoded_byte_align", compressed->params.u.fax.encoded_byte_align);
				s_write_attribute_int(ctx, dev->out, "end_of_block", compressed->params.u.fax.end_of_block);
				s_write_attribute_int(ctx, dev->out, "black_is_1", compressed->params.u.fax.black_is_1);
				s_write_attribute_int(ctx, dev->out, "damaged_rows_before_error", compressed->params.u.fax.damaged_rows_before_error);
			}
			else if (compressed->params.type == FZ_IMAGE_FLATE)
			{
				type = "flate";
				s_write_attribute_string(ctx, dev->out, "type", type);
				s_write_attribute_int(ctx, dev->out, "columns", compressed->params.u.flate.columns);
				s_write_attribute_int(ctx, dev->out, "colors", compressed->params.u.flate.colors);
				s_write_attribute_int(ctx, dev->out, "predictor", compressed->params.u.flate.predictor);
				s_write_attribute_int(ctx, dev->out, "bpc", compressed->params.u.flate.bpc);
			}
			else if (compressed->params.type == FZ_IMAGE_BROTLI)
			{
				type = "brotli";
				s_write_attribute_string(ctx, dev->out, "type", type);
				s_write_attribute_int(ctx, dev->out, "columns", compressed->params.u.brotli.columns);
				s_write_attribute_int(ctx, dev->out, "colors", compressed->params.u.brotli.colors);
				s_write_attribute_int(ctx, dev->out, "predictor", compressed->params.u.brotli.predictor);
				s_write_attribute_int(ctx, dev->out, "bpc", compressed->params.u.brotli.bpc);
			}
			else if (compressed->params.type == FZ_IMAGE_LZW)
			{
				type = "lzw";
				s_write_attribute_string(ctx, dev->out, "type", type);
				s_write_attribute_int(ctx, dev->out, "columns", compressed->params.u.lzw.columns);
				s_write_attribute_int(ctx, dev->out, "colors", compressed->params.u.lzw.colors);
				s_write_attribute_int(ctx, dev->out, "predictor", compressed->params.u.lzw.predictor);
				s_write_attribute_int(ctx, dev->out, "bpc", compressed->params.u.lzw.bpc);
				s_write_attribute_int(ctx, dev->out, "early_change", compressed->params.u.lzw.early_change);
			}
			else if (compressed->params.type == FZ_IMAGE_BMP)
			{
				type = "bmp";
				s_write_attribute_string(ctx, dev->out, "type", type);
			}
			else if (compressed->params.type == FZ_IMAGE_GIF)
			{
				type = "gif";
				s_write_attribute_string(ctx, dev->out, "type", type);
			}
			else if (compressed->params.type == FZ_IMAGE_JBIG2)
			{
				type = "jbig2";
				s_write_attribute_string(ctx, dev->out, "type", type);
				/* do we need to write out *compressed->params.globals somehow? */
			}
			else if (compressed->params.type == FZ_IMAGE_JPEG)
			{
				type = "jpeg";
				s_write_attribute_string(ctx, dev->out, "type", type);
				s_write_attribute_int(ctx, dev->out, "color_transform", compressed->params.u.jpeg.color_transform);
				if (compressed->params.u.jpeg.invert_cmyk)
					s_write_attribute_int(ctx, dev->out, "invert_cmyk", 1);
			}
			else if (compressed->params.type == FZ_IMAGE_JPX)
			{
				type = "jpx";
				s_write_attribute_string(ctx, dev->out, "type", type);
				s_write_attribute_int(ctx, dev->out, "smask_in_data", compressed->params.u.jpx.smask_in_data);
			}
			else if (compressed->params.type == FZ_IMAGE_JXR)
			{
				type = "jxr";
				s_write_attribute_string(ctx, dev->out, "type", type);
			}
			else if (compressed->params.type == FZ_IMAGE_PNG)
			{
				type = "png";
				s_write_attribute_string(ctx, dev->out, "type", type);
			}
			else if (compressed->params.type == FZ_IMAGE_PNM)
			{
				type = "pnm";
				s_write_attribute_string(ctx, dev->out, "type", type);
			}
			else if (compressed->params.type == FZ_IMAGE_TIFF)
			{
				type = "tiff";
				s_write_attribute_string(ctx, dev->out, "type", type);
			}
			else
			{
				/* Unrecognised. */
			}

			if (type)
			{
				/* Write out raw data. */
				unsigned char *data;
				size_t	datasize = fz_buffer_storage(ctx, compressed->buffer, &data);
				size_t i;
				s_write_attribute_size(ctx, dev->out, "datasize", datasize);
				s_xml_starttag_end(ctx, dev->out);
				for (i=0; i<datasize; ++i)
				{
					if (i % 32 == 0) fz_write_printf(ctx, dev->out, "\n   ");
					if (i % 4 == 0) fz_write_printf(ctx, dev->out, " ");
					fz_write_printf(ctx, dev->out, "%02x", data[i]);
				}
				fz_write_printf(ctx, dev->out, "\n");
			}
		}

		if (!type)
		{
			/* Compressed data not available, so write out raw pixel values. */
			int l2factor = 0;
			int y;
			s_write_attribute_string(ctx, dev->out, "type", "pixmap");
			s_xml_starttag_end(ctx, dev->out);
			pixmap = img->get_pixmap(ctx, img, NULL /*subarea*/, img->w, img->h, &l2factor);
			s_write_attribute_int(ctx, dev->out, "x", pixmap->x);
			s_write_attribute_int(ctx, dev->out, "y", pixmap->y);
			s_write_attribute_int(ctx, dev->out, "w", pixmap->w);
			s_write_attribute_int(ctx, dev->out, "h", pixmap->h);
			s_write_attribute_int(ctx, dev->out, "n", pixmap->n);
			s_write_attribute_int(ctx, dev->out, "s", pixmap->s);
			s_write_attribute_int(ctx, dev->out, "alpha", pixmap->alpha);
			s_write_attribute_int(ctx, dev->out, "flags", pixmap->flags);
			s_write_attribute_int(ctx, dev->out, "xres", pixmap->xres);
			s_write_attribute_int(ctx, dev->out, "yres", pixmap->yres);
			s_write_attribute_matrix(ctx, dev->out, "ctm", &ctm);
			s_xml_starttag_end(ctx, dev->out);
			for (y=0; y<pixmap->h; ++y)
			{
				int x;
				s_xml_starttag_begin(ctx, dev->out, "line");
				s_write_attribute_int(ctx, dev->out, "y", y);
				s_xml_starttag_end(ctx, dev->out);
				for (x=0; x<pixmap->w; ++x)
				{
					int b;
					fz_write_printf(ctx, dev->out, " ");
					for (b=0; b<pixmap->n; ++b)
					{
						fz_write_printf(ctx, dev->out, "%02x", pixmap->samples[y*(size_t)pixmap->stride + x*(size_t)pixmap->n + b]);
					}
				}
				s_xml_endtag(ctx, dev->out, "line");
			}
		}
		s_xml_endtag(ctx, dev->out, "image");
	}
	fz_always(ctx)
	{
		fz_drop_pixmap(ctx, pixmap);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

fz_device *fz_new_xmltext_device(fz_context *ctx, fz_output *out)
{
	fz_xmltext_device *dev = fz_new_derived_device(ctx, fz_xmltext_device);

	dev->super.close_device = fz_stext_close_device;

	dev->super.fill_text = fz_xmltext_fill_text;
	dev->super.stroke_text = fz_xmltext_stroke_text;
	dev->super.clip_text = fz_xmltext_clip_text;
	dev->super.clip_stroke_text = fz_xmltext_clip_stroke_text;
	dev->super.ignore_text = fz_xmltext_ignore_text;
	dev->super.fill_image = fz_xmltext_fill_image;

	dev->out = out;

	return (fz_device*)dev;
}
