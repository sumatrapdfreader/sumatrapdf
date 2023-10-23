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

#define SUBSCRIPT_OFFSET 0.2f
#define SUPERSCRIPT_OFFSET -0.2f

#include <ft2build.h>
#include FT_FREETYPE_H

// Text black color when converted from DeviceCMYK to RGB
#define CMYK_BLACK 0x221f1f

static void fz_scale_stext_page(fz_context *ctx, fz_stext_page *page, float scale)
{
	fz_matrix m = fz_scale(scale, scale);
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;

	for (block = page->first_block; block; block = block->next)
	{
		block->bbox = fz_transform_rect(block->bbox, m);
		switch (block->type)
		{
		case FZ_STEXT_BLOCK_TEXT:
			for (line = block->u.t.first_line; line; line = line->next)
			{
				line->bbox = fz_transform_rect(block->bbox, m);
				for (ch = line->first_char; ch; ch = ch->next)
				{
					ch->origin = fz_transform_point(ch->origin, m);
					ch->quad = fz_transform_quad(ch->quad, m);
					ch->size = ch->size * scale;
				}
			}
			break;

		case FZ_STEXT_BLOCK_IMAGE:
			block->u.i.transform = fz_post_scale(block->u.i.transform, scale, scale);
			break;
		}
	}
}

/* HTML output (visual formatting with preserved layout) */

static int
detect_super_script(fz_stext_line *line, fz_stext_char *ch)
{
	if (line->wmode == 0 && line->dir.x == 1 && line->dir.y == 0)
		return ch->origin.y < line->first_char->origin.y - ch->size * 0.1f;
	return 0;
}

static const char *
font_full_name(fz_context *ctx, fz_font *font)
{
	const char *name = fz_font_name(ctx, font);
	const char *s = strchr(name, '+');
	return s ? s + 1 : name;
}

static const char *
html_clean_font_name(const char *fontname)
{
	if (strstr(fontname, "Times"))
		return "Times New Roman";
	if (strstr(fontname, "Arial") || strstr(fontname, "Helvetica"))
	{
		if (strstr(fontname, "Narrow") || strstr(fontname, "Condensed"))
			return "Arial Narrow";
		return "Arial";
	}
	if (strstr(fontname, "Courier"))
		return "Courier";
	return fontname;
}

static void
font_family_name(fz_context *ctx, fz_font *font, char *buf, int size, int is_mono, int is_serif)
{
	const char *name = html_clean_font_name(font_full_name(ctx, font));
	char *s;
	fz_strlcpy(buf, name, size);
	s = strrchr(buf, '-');
	if (s)
		*s = 0;
	if (is_mono)
		fz_strlcat(buf, ",monospace", size);
	else
		fz_strlcat(buf, is_serif ? ",serif" : ",sans-serif", size);
}

static void
fz_print_style_begin_html(fz_context *ctx, fz_output *out, fz_font *font, float size, int sup, int color)
{
	char family[80];

	int is_bold = fz_font_is_bold(ctx, font);
	int is_italic = fz_font_is_italic(ctx, font);
	int is_serif = fz_font_is_serif(ctx, font);
	int is_mono = fz_font_is_monospaced(ctx, font);

	font_family_name(ctx, font, family, sizeof family, is_mono, is_serif);

	if (sup) fz_write_string(ctx, out, "<sup>");
	if (is_mono) fz_write_string(ctx, out, "<tt>");
	if (is_bold) fz_write_string(ctx, out, "<b>");
	if (is_italic) fz_write_string(ctx, out, "<i>");
	fz_write_printf(ctx, out, "<span style=\"font-family:%s;font-size:%.1fpt", family, size);
	if (color != 0 && color != CMYK_BLACK)
		fz_write_printf(ctx, out, ";color:#%06x", color);
	fz_write_printf(ctx, out, "\">");
}

static void
fz_print_style_end_html(fz_context *ctx, fz_output *out, fz_font *font, float size, int sup, int color)
{
	int is_mono = fz_font_is_monospaced(ctx, font);
	int is_bold = fz_font_is_bold(ctx,font);
	int is_italic = fz_font_is_italic(ctx, font);

	fz_write_string(ctx, out, "</span>");
	if (is_italic) fz_write_string(ctx, out, "</i>");
	if (is_bold) fz_write_string(ctx, out, "</b>");
	if (is_mono) fz_write_string(ctx, out, "</tt>");
	if (sup) fz_write_string(ctx, out, "</sup>");
}

static void
fz_print_stext_image_as_html(fz_context *ctx, fz_output *out, fz_stext_block *block)
{
	fz_matrix ctm = block->u.i.transform;

#define USE_CSS_MATRIX_TRANSFORMS
#ifdef USE_CSS_MATRIX_TRANSFORMS
	/* Matrix maths notes.
	 * When we get here ctm maps the unit square to the position in device
	 * space occupied by the image.
	 *
	 * That is to say that mapping the 4 corners of the unit square through
	 * the transform, give us the 4 target corners. We extend the corners
	 * by adding an extra '1' into them to allow transforms to work. Thus
	 * (x,y) maps through ctm = (a b c d e f) as:
	 *
	 * (x y 1) (a b 0) = (X Y 1)
	 *         (c d 0)
	 *         (e f 1)
	 *
	 * To simplify reading of matrix maths, we use the trick where we
	 * 'drop' the first matrix down the page. Thus the corners c0=(0,0),
	 * c1=(1,0), c2=(1,1), c3=(0,1) map to C0, C1, C2, C3 respectively:
	 *
	 *         (    a     b 0)
	 *         (    c     d 0)
	 *         (    e     f 1)
	 * (0 0 1) (    e     f 1)
	 * (0 1 1) (  c+e   d+f 1)
	 * (1 1 1) (a+c+e b+d+f 1)
	 * (1 0 1) (  a+e   b+f 1)
	 *
	 * where C0 = (e,f), C1=(c+e, d+f) C2=(a+c+e, b+d+f), C3=(a+e, b+f)
	 *
	 * Unfortunately, the CSS matrix transform, does not map the unit square.
	 * Rather it does something moderately mad. As far as I can work out, the
	 * top left corner of a (0,0) -> (w, h) box is transformed using the .e
	 * and .f entries of the matrix. Then the image from within that square
	 * is transformed using the centre of that square as the origin.
	 *
	 * So, an image placed at (0,0) in destination space with 1:1 transform
	 * will result in an image a (0,0) as you'd expect. But an image at (0,0)
	 * with a scale of 2, will result in 25% of the image off the left of the
	 * screen, and 25% off the top.
	 *
	 * Accordingly, we have to adjust the ctm in several steps.
	 */
	/* Move to moving the centre of the image. */
	ctm.e += (ctm.a+ctm.c)/2;
	ctm.f += (ctm.b+ctm.d)/2;
	/* Move from transforming the unit square to w/h */
	ctm.a /= block->u.i.image->w;
	ctm.b /= block->u.i.image->w;
	ctm.c /= block->u.i.image->h;
	ctm.d /= block->u.i.image->h;
	/* Move from points to pixels */
	ctm.a *= 96.0f/72;
	ctm.b *= 96.0f/72;
	ctm.c *= 96.0f/72;
	ctm.d *= 96.0f/72;
	ctm.e *= 96.0f/72;
	ctm.f *= 96.0f/72;
	/* Move to moving the top left of the untransformed image box, cos HTML is bonkers. */
	ctm.e -= block->u.i.image->w/2;
	ctm.f -= block->u.i.image->h/2;

	fz_write_printf(ctx, out, "<img style=\"position:absolute;transform:matrix(%g,%g,%g,%g,%g,%g)\" src=\"",
		ctm.a, ctm.b, ctm.c, ctm.d, ctm.e, ctm.f);
#else
	/* Alternative version of the code that uses scaleX/Y and rotate
	 * instead, but only copes with axis aligned cases. */
	int t;

	int x = block->bbox.x0;
	int y = block->bbox.y0;
	int w = block->bbox.x1 - block->bbox.x0;
	int h = block->bbox.y1 - block->bbox.y0;

	const char *flip = "";

	if (ctm.b == 0 && ctm.c == 0)
	{
		if (ctm.a < 0 && ctm.d < 0)
			flip = "transform: scaleX(-1) scaleY(-1);";
		else if (ctm.a < 0)
		{
			flip = "transform: scaleX(-1);";
		}
		else if (ctm.d < 0)
		{
			flip = "transform: scaleY(-1);";
		}
	} else if (ctm.a == 0 && ctm.d == 0) {
		if (ctm.b < 0 && ctm.c < 0)
		{
			flip = "transform: scaleY(-1) rotate(90deg);";
			x += (w-h)/2;
			y -= (w-h)/2;
			t = w; w = h; h = t;
		}
		else if (ctm.b < 0)
		{
			flip = "transform: scaleX(-1) scaleY(-1) rotate(90deg);";
			x += (w-h)/2;
			y -= (w-h)/2;
			t = w; w = h; h = t;
		}
		else if (ctm.c < 0)
		{
			flip = "transform: scaleX(-1) scaleY(-1) rotate(270deg);";
			x += (w-h)/2;
			y -= (w-h)/2;
			t = w; w = h; h = t;
		}
		else
		{
			flip = "transform: scaleY(-1) rotate(270deg);";
			x += (w-h)/2;
			y -= (w-h)/2;
			t = w; w = h; h = t;
		}
	}

	fz_write_printf(ctx, out, "<img style=\"position:absolute;%stop:%dpt;left:%dpt;width:%dpt;height:%dpt\" src=\"", flip, y, x, w, h);
#endif
	fz_write_image_as_data_uri(ctx, out, block->u.i.image);
	fz_write_string(ctx, out, "\">\n");
}

void
fz_print_stext_block_as_html(fz_context *ctx, fz_output *out, fz_stext_block *block)
{
	fz_stext_line *line;
	fz_stext_char *ch;
	float x, y, h;

	fz_font *font = NULL;
	float size = 0;
	int sup = 0;
	int color = 0;

	for (line = block->u.t.first_line; line; line = line->next)
	{
		x = line->bbox.x0;
		y = line->bbox.y0;
		h = line->bbox.y1 - line->bbox.y0;

		if (line->first_char)
		{
			h = line->first_char->size;
			y = line->first_char->origin.y - h * 0.8f;
		}

		fz_write_printf(ctx, out, "<p style=\"top:%.1fpt;left:%.1fpt;line-height:%.1fpt\">", y, x, h);
		font = NULL;

		for (ch = line->first_char; ch; ch = ch->next)
		{
			int ch_sup = detect_super_script(line, ch);
			if (ch->font != font || ch->size != size || ch_sup != sup || ch->color != color)
			{
				if (font)
					fz_print_style_end_html(ctx, out, font, size, sup, color);
				font = ch->font;
				size = ch->size;
				color = ch->color;
				sup = ch_sup;
				fz_print_style_begin_html(ctx, out, font, size, sup, color);
			}

			switch (ch->c)
			{
			default:
				if (ch->c >= 32 && ch->c <= 127)
					fz_write_byte(ctx, out, ch->c);
				else
					fz_write_printf(ctx, out, "&#x%x;", ch->c);
				break;
			case '<': fz_write_string(ctx, out, "&lt;"); break;
			case '>': fz_write_string(ctx, out, "&gt;"); break;
			case '&': fz_write_string(ctx, out, "&amp;"); break;
			case '"': fz_write_string(ctx, out, "&quot;"); break;
			case '\'': fz_write_string(ctx, out, "&apos;"); break;
			}
		}

		if (font)
			fz_print_style_end_html(ctx, out, font, size, sup, color);

		fz_write_string(ctx, out, "</p>\n");
	}
}

void
fz_print_stext_page_as_html(fz_context *ctx, fz_output *out, fz_stext_page *page, int id)
{
	fz_stext_block *block;

	float w = page->mediabox.x1 - page->mediabox.x0;
	float h = page->mediabox.y1 - page->mediabox.y0;

	fz_write_printf(ctx, out, "<div id=\"page%d\" style=\"width:%.1fpt;height:%.1fpt\">\n", id, w, h);

	for (block = page->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_IMAGE)
			fz_print_stext_image_as_html(ctx, out, block);
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
			fz_print_stext_block_as_html(ctx, out, block);
	}

	fz_write_string(ctx, out, "</div>\n");
}

void
fz_print_stext_header_as_html(fz_context *ctx, fz_output *out)
{
	fz_write_string(ctx, out, "<!DOCTYPE html>\n");
	fz_write_string(ctx, out, "<html>\n");
	fz_write_string(ctx, out, "<head>\n");
	fz_write_string(ctx, out, "<style>\n");
	fz_write_string(ctx, out, "body{background-color:slategray}\n");
	fz_write_string(ctx, out, "div{position:relative;background-color:white;margin:1em auto;box-shadow:1px 1px 8px -2px black}\n");
	fz_write_string(ctx, out, "p{position:absolute;white-space:pre;margin:0}\n");
	fz_write_string(ctx, out, "</style>\n");
	fz_write_string(ctx, out, "</head>\n");
	fz_write_string(ctx, out, "<body>\n");
}

void
fz_print_stext_trailer_as_html(fz_context *ctx, fz_output *out)
{
	fz_write_string(ctx, out, "</body>\n");
	fz_write_string(ctx, out, "</html>\n");
}

/* XHTML output (semantic, little layout, suitable for reflow) */

static void
fz_print_stext_image_as_xhtml(fz_context *ctx, fz_output *out, fz_stext_block *block)
{
	int w = block->bbox.x1 - block->bbox.x0;
	int h = block->bbox.y1 - block->bbox.y0;

	fz_write_printf(ctx, out, "<p><img width=\"%d\" height=\"%d\" src=\"", w, h);
	fz_write_image_as_data_uri(ctx, out, block->u.i.image);
	fz_write_string(ctx, out, "\"/></p>\n");
}

static void
fz_print_style_begin_xhtml(fz_context *ctx, fz_output *out, fz_font *font, int sup)
{
	int is_mono = fz_font_is_monospaced(ctx, font);
	int is_bold = fz_font_is_bold(ctx, font);
	int is_italic = fz_font_is_italic(ctx, font);

	if (sup)
		fz_write_string(ctx, out, "<sup>");
	if (is_mono)
		fz_write_string(ctx, out, "<tt>");
	if (is_bold)
		fz_write_string(ctx, out, "<b>");
	if (is_italic)
		fz_write_string(ctx, out, "<i>");
}

static void
fz_print_style_end_xhtml(fz_context *ctx, fz_output *out, fz_font *font, int sup)
{
	int is_mono = fz_font_is_monospaced(ctx, font);
	int is_bold = fz_font_is_bold(ctx, font);
	int is_italic = fz_font_is_italic(ctx, font);

	if (is_italic)
		fz_write_string(ctx, out, "</i>");
	if (is_bold)
		fz_write_string(ctx, out, "</b>");
	if (is_mono)
		fz_write_string(ctx, out, "</tt>");
	if (sup)
		fz_write_string(ctx, out, "</sup>");
}

static float avg_font_size_of_line(fz_stext_char *ch)
{
	float size = 0;
	int n = 0;
	if (!ch)
		return 0;
	while (ch)
	{
		size += ch->size;
		++n;
		ch = ch->next;
	}
	return size / n;
}

static const char *tag_from_font_size(float size)
{
	if (size >= 20) return "h1";
	if (size >= 15) return "h2";
	if (size >= 12) return "h3";
	return "p";
}

static void fz_print_stext_block_as_xhtml(fz_context *ctx, fz_output *out, fz_stext_block *block)
{
	fz_stext_line *line;
	fz_stext_char *ch;

	fz_font *font = NULL;
	int sup = 0;
	int sp = 1;
	const char *tag = NULL;
	const char *new_tag;

	for (line = block->u.t.first_line; line; line = line->next)
	{
		new_tag = tag_from_font_size(avg_font_size_of_line(line->first_char));
		if (tag != new_tag)
		{
			if (tag)
			{
				if (font)
					fz_print_style_end_xhtml(ctx, out, font, sup);
				fz_write_printf(ctx, out, "</%s>", tag);
			}
			tag = new_tag;
			fz_write_printf(ctx, out, "<%s>", tag);
			if (font)
				fz_print_style_begin_xhtml(ctx, out, font, sup);
		}

		if (!sp)
			fz_write_byte(ctx, out, ' ');

		for (ch = line->first_char; ch; ch = ch->next)
		{
			int ch_sup = detect_super_script(line, ch);
			if (ch->font != font || ch_sup != sup)
			{
				if (font)
					fz_print_style_end_xhtml(ctx, out, font, sup);
				font = ch->font;
				sup = ch_sup;
				fz_print_style_begin_xhtml(ctx, out, font, sup);
			}

			sp = (ch->c == ' ');
			switch (ch->c)
			{
			default:
				if (ch->c >= 32 && ch->c <= 127)
					fz_write_byte(ctx, out, ch->c);
				else
					fz_write_printf(ctx, out, "&#x%x;", ch->c);
				break;
			case '<': fz_write_string(ctx, out, "&lt;"); break;
			case '>': fz_write_string(ctx, out, "&gt;"); break;
			case '&': fz_write_string(ctx, out, "&amp;"); break;
			case '"': fz_write_string(ctx, out, "&quot;"); break;
			case '\'': fz_write_string(ctx, out, "&apos;"); break;
			}
		}
	}

	if (font)
		fz_print_style_end_xhtml(ctx, out, font, sup);
	fz_write_printf(ctx, out, "</%s>\n", tag);
}

void
fz_print_stext_page_as_xhtml(fz_context *ctx, fz_output *out, fz_stext_page *page, int id)
{
	fz_stext_block *block;

	fz_write_printf(ctx, out, "<div id=\"page%d\">\n", id);

	for (block = page->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_IMAGE)
			fz_print_stext_image_as_xhtml(ctx, out, block);
		else if (block->type == FZ_STEXT_BLOCK_TEXT)
			fz_print_stext_block_as_xhtml(ctx, out, block);
	}

	fz_write_string(ctx, out, "</div>\n");
}

void
fz_print_stext_header_as_xhtml(fz_context *ctx, fz_output *out)
{
	fz_write_string(ctx, out, "<?xml version=\"1.0\"?>\n");
	fz_write_string(ctx, out, "<!DOCTYPE html");
	fz_write_string(ctx, out, " PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"");
	fz_write_string(ctx, out, " \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n");
	fz_write_string(ctx, out, "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n");
	fz_write_string(ctx, out, "<head>\n");
	fz_write_string(ctx, out, "<style>\n");
	fz_write_string(ctx, out, "p{white-space:pre-wrap}\n");
	fz_write_string(ctx, out, "</style>\n");
	fz_write_string(ctx, out, "</head>\n");
	fz_write_string(ctx, out, "<body>\n");
}

void
fz_print_stext_trailer_as_xhtml(fz_context *ctx, fz_output *out)
{
	fz_write_string(ctx, out, "</body>\n");
	fz_write_string(ctx, out, "</html>\n");
}

/* Detailed XML dump of the entire structured text data */

void
fz_print_stext_page_as_xml(fz_context *ctx, fz_output *out, fz_stext_page *page, int id)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;

	fz_write_printf(ctx, out, "<page id=\"page%d\" width=\"%g\" height=\"%g\">\n", id,
		page->mediabox.x1 - page->mediabox.x0,
		page->mediabox.y1 - page->mediabox.y0);

	for (block = page->first_block; block; block = block->next)
	{
		switch (block->type)
		{
		case FZ_STEXT_BLOCK_TEXT:
			fz_write_printf(ctx, out, "<block bbox=\"%g %g %g %g\">\n",
					block->bbox.x0, block->bbox.y0, block->bbox.x1, block->bbox.y1);
			for (line = block->u.t.first_line; line; line = line->next)
			{
				fz_font *font = NULL;
				float size = 0;
				const char *name = NULL;

				fz_write_printf(ctx, out, "<line bbox=\"%g %g %g %g\" wmode=\"%d\" dir=\"%g %g\">\n",
						line->bbox.x0, line->bbox.y0, line->bbox.x1, line->bbox.y1,
						line->wmode,
						line->dir.x, line->dir.y);

				for (ch = line->first_char; ch; ch = ch->next)
				{
					if (ch->font != font || ch->size != size)
					{
						if (font)
							fz_write_string(ctx, out, "</font>\n");
						font = ch->font;
						size = ch->size;
						name = font_full_name(ctx, font);
						fz_write_printf(ctx, out, "<font name=\"%s\" size=\"%g\">\n", name, size);
					}
					fz_write_printf(ctx, out, "<char quad=\"%g %g %g %g %g %g %g %g\" x=\"%g\" y=\"%g\" bidi=\"%d\" color=\"#%06x\" c=\"",
							ch->quad.ul.x, ch->quad.ul.y,
							ch->quad.ur.x, ch->quad.ur.y,
							ch->quad.ll.x, ch->quad.ll.y,
							ch->quad.lr.x, ch->quad.lr.y,
							ch->origin.x, ch->origin.y,
							ch->bidi,
							ch->color);
					switch (ch->c)
					{
					case '<': fz_write_string(ctx, out, "&lt;"); break;
					case '>': fz_write_string(ctx, out, "&gt;"); break;
					case '&': fz_write_string(ctx, out, "&amp;"); break;
					case '"': fz_write_string(ctx, out, "&quot;"); break;
					case '\'': fz_write_string(ctx, out, "&apos;"); break;
					default:
						   if (ch->c >= 32 && ch->c <= 127)
							   fz_write_printf(ctx, out, "%c", ch->c);
						   else
							   fz_write_printf(ctx, out, "&#x%x;", ch->c);
						   break;
					}
					fz_write_string(ctx, out, "\"/>\n");
				}

				if (font)
					fz_write_string(ctx, out, "</font>\n");

				fz_write_string(ctx, out, "</line>\n");
			}
			fz_write_string(ctx, out, "</block>\n");
			break;

		case FZ_STEXT_BLOCK_IMAGE:
			fz_write_printf(ctx, out, "<image bbox=\"%g %g %g %g\" />\n",
					block->bbox.x0, block->bbox.y0, block->bbox.x1, block->bbox.y1);
			break;
		}
	}
	fz_write_string(ctx, out, "</page>\n");
}

/* JSON dump */

void
fz_print_stext_page_as_json(fz_context *ctx, fz_output *out, fz_stext_page *page, float scale)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;

	fz_write_printf(ctx, out, "{%q:[", "blocks");

	for (block = page->first_block; block; block = block->next)
	{
		if (block != page->first_block)
			fz_write_string(ctx, out, ",");
		switch (block->type)
		{
		case FZ_STEXT_BLOCK_TEXT:
			fz_write_printf(ctx, out, "{%q:%q,", "type", "text");
			fz_write_printf(ctx, out, "%q:{", "bbox");
			fz_write_printf(ctx, out, "%q:%d,", "x", (int)(block->bbox.x0 * scale));
			fz_write_printf(ctx, out, "%q:%d,", "y", (int)(block->bbox.y0 * scale));
			fz_write_printf(ctx, out, "%q:%d,", "w", (int)((block->bbox.x1 - block->bbox.x0) * scale));
			fz_write_printf(ctx, out, "%q:%d},", "h", (int)((block->bbox.y1 - block->bbox.y0) * scale));
			fz_write_printf(ctx, out, "%q:[", "lines");

			for (line = block->u.t.first_line; line; line = line->next)
			{
				if (line != block->u.t.first_line)
					fz_write_string(ctx, out, ",");
				fz_write_printf(ctx, out, "{%q:%d,", "wmode", line->wmode);
				fz_write_printf(ctx, out, "%q:{", "bbox");
				fz_write_printf(ctx, out, "%q:%d,", "x", (int)(line->bbox.x0 * scale));
				fz_write_printf(ctx, out, "%q:%d,", "y", (int)(line->bbox.y0 * scale));
				fz_write_printf(ctx, out, "%q:%d,", "w", (int)((line->bbox.x1 - line->bbox.x0) * scale));
				fz_write_printf(ctx, out, "%q:%d},", "h", (int)((line->bbox.y1 - line->bbox.y0) * scale));

				/* Since we force preserve-spans, the first char has the style for the entire line. */
				if (line->first_char)
				{
					fz_font *font = line->first_char->font;
					char *font_family = "sans-serif";
					char *font_weight = "normal";
					char *font_style = "normal";
					if (fz_font_is_monospaced(ctx, font)) font_family = "monospace";
					else if (fz_font_is_serif(ctx, font)) font_family = "serif";
					if (fz_font_is_bold(ctx, font)) font_weight = "bold";
					if (fz_font_is_italic(ctx, font)) font_style = "italic";
					fz_write_printf(ctx, out, "%q:{", "font");
					fz_write_printf(ctx, out, "%q:%q,", "name", fz_font_name(ctx, font));
					fz_write_printf(ctx, out, "%q:%q,", "family", font_family);
					fz_write_printf(ctx, out, "%q:%q,", "weight", font_weight);
					fz_write_printf(ctx, out, "%q:%q,", "style", font_style);
					fz_write_printf(ctx, out, "%q:%d},", "size", (int)(line->first_char->size * scale));
					fz_write_printf(ctx, out, "%q:%d,", "x", (int)(line->first_char->origin.x * scale));
					fz_write_printf(ctx, out, "%q:%d,", "y", (int)(line->first_char->origin.y * scale));
				}

				fz_write_printf(ctx, out, "%q:\"", "text");
				for (ch = line->first_char; ch; ch = ch->next)
				{
					if (ch->c == '"' || ch->c == '\\')
						fz_write_printf(ctx, out, "\\%c", ch->c);
					else if (ch->c < 32)
						fz_write_printf(ctx, out, "\\u%04x", ch->c);
					else
						fz_write_printf(ctx, out, "%C", ch->c);
				}
				fz_write_printf(ctx, out, "\"}");
			}
			fz_write_string(ctx, out, "]}");
			break;

		case FZ_STEXT_BLOCK_IMAGE:
			fz_write_printf(ctx, out, "{%q:%q,", "type", "image");
			fz_write_printf(ctx, out, "%q:{", "bbox");
			fz_write_printf(ctx, out, "%q:%d,", "x", (int)(block->bbox.x0 * scale));
			fz_write_printf(ctx, out, "%q:%d,", "y", (int)(block->bbox.y0 * scale));
			fz_write_printf(ctx, out, "%q:%d,", "w", (int)((block->bbox.x1 - block->bbox.x0) * scale));
			fz_write_printf(ctx, out, "%q:%d}}", "h", (int)((block->bbox.y1 - block->bbox.y0) * scale));
			break;
		}
	}
	fz_write_string(ctx, out, "]}");
}

/* Plain text */

void
fz_print_stext_page_as_text(fz_context *ctx, fz_output *out, fz_stext_page *page)
{
	fz_stext_block *block;
	fz_stext_line *line;
	fz_stext_char *ch;
	char utf[10];
	int i, n;

	for (block = page->first_block; block; block = block->next)
	{
		if (block->type == FZ_STEXT_BLOCK_TEXT)
		{
			for (line = block->u.t.first_line; line; line = line->next)
			{
				for (ch = line->first_char; ch; ch = ch->next)
				{
					n = fz_runetochar(utf, ch->c);
					for (i = 0; i < n; i++)
						fz_write_byte(ctx, out, utf[i]);
				}
				fz_write_string(ctx, out, "\n");
			}
			fz_write_string(ctx, out, "\n");
		}
	}
}

/* Text output writer */

enum {
	FZ_FORMAT_TEXT,
	FZ_FORMAT_HTML,
	FZ_FORMAT_XHTML,
	FZ_FORMAT_STEXT_XML,
	FZ_FORMAT_STEXT_JSON,
};

typedef struct
{
	fz_document_writer super;
	int format;
	int number;
	fz_stext_options opts;
	fz_stext_page *page;
	fz_output *out;
} fz_text_writer;

static fz_device *
text_begin_page(fz_context *ctx, fz_document_writer *wri_, fz_rect mediabox)
{
	fz_text_writer *wri = (fz_text_writer*)wri_;
	float s = wri->opts.scale;

	if (wri->page)
	{
		fz_drop_stext_page(ctx, wri->page);
		wri->page = NULL;
	}

	wri->number++;

	wri->page = fz_new_stext_page(ctx, fz_transform_rect(mediabox, fz_scale(s, s)));
	return fz_new_stext_device(ctx, wri->page, &wri->opts);
}

static void
text_end_page(fz_context *ctx, fz_document_writer *wri_, fz_device *dev)
{
	fz_text_writer *wri = (fz_text_writer*)wri_;
	float s = wri->opts.scale;

	fz_scale_stext_page(ctx, wri->page, s);

	fz_try(ctx)
	{
		fz_close_device(ctx, dev);
		switch (wri->format)
		{
		default:
		case FZ_FORMAT_TEXT:
			fz_print_stext_page_as_text(ctx, wri->out, wri->page);
			break;
		case FZ_FORMAT_HTML:
			fz_print_stext_page_as_html(ctx, wri->out, wri->page, wri->number);
			break;
		case FZ_FORMAT_XHTML:
			fz_print_stext_page_as_xhtml(ctx, wri->out, wri->page, wri->number);
			break;
		case FZ_FORMAT_STEXT_XML:
			fz_print_stext_page_as_xml(ctx, wri->out, wri->page, wri->number);
			break;
		case FZ_FORMAT_STEXT_JSON:
			if (wri->number > 1)
				fz_write_string(ctx, wri->out, ",");
			fz_print_stext_page_as_json(ctx, wri->out, wri->page, 1);
			break;
		}
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_stext_page(ctx, wri->page);
		wri->page = NULL;
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
text_close_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_text_writer *wri = (fz_text_writer*)wri_;
	switch (wri->format)
	{
	case FZ_FORMAT_HTML:
		fz_print_stext_trailer_as_html(ctx, wri->out);
		break;
	case FZ_FORMAT_XHTML:
		fz_print_stext_trailer_as_xhtml(ctx, wri->out);
		break;
	case FZ_FORMAT_STEXT_XML:
		fz_write_string(ctx, wri->out, "</document>\n");
		break;
	case FZ_FORMAT_STEXT_JSON:
		fz_write_string(ctx, wri->out, "]\n");
		break;
	}
	fz_close_output(ctx, wri->out);
}

static void
text_drop_writer(fz_context *ctx, fz_document_writer *wri_)
{
	fz_text_writer *wri = (fz_text_writer*)wri_;
	fz_drop_stext_page(ctx, wri->page);
	fz_drop_output(ctx, wri->out);
}

fz_document_writer *
fz_new_text_writer_with_output(fz_context *ctx, const char *format, fz_output *out, const char *options)
{
	fz_text_writer *wri = NULL;

	fz_var(wri);

	fz_try(ctx)
	{
		wri = fz_new_derived_document_writer(ctx, fz_text_writer, text_begin_page, text_end_page, text_close_writer, text_drop_writer);
		fz_parse_stext_options(ctx, &wri->opts, options);

		wri->format = FZ_FORMAT_TEXT;
		if (!strcmp(format, "text"))
			wri->format = FZ_FORMAT_TEXT;
		else if (!strcmp(format, "html"))
			wri->format = FZ_FORMAT_HTML;
		else if (!strcmp(format, "xhtml"))
			wri->format = FZ_FORMAT_XHTML;
		else if (!strcmp(format, "stext"))
			wri->format = FZ_FORMAT_STEXT_XML;
		else if (!strcmp(format, "stext.xml"))
			wri->format = FZ_FORMAT_STEXT_XML;
		else if (!strcmp(format, "stext.json"))
		{
			wri->format = FZ_FORMAT_STEXT_JSON;
			wri->opts.flags |= FZ_STEXT_PRESERVE_SPANS;
		}

		wri->out = out;

		switch (wri->format)
		{
		case FZ_FORMAT_HTML:
			fz_print_stext_header_as_html(ctx, wri->out);
			break;
		case FZ_FORMAT_XHTML:
			fz_print_stext_header_as_xhtml(ctx, wri->out);
			break;
		case FZ_FORMAT_STEXT_XML:
			fz_write_string(ctx, wri->out, "<?xml version=\"1.0\"?>\n");
			fz_write_string(ctx, wri->out, "<document>\n");
			break;
		case FZ_FORMAT_STEXT_JSON:
			fz_write_string(ctx, wri->out, "[");
			break;
		}
	}
	fz_catch(ctx)
	{
		fz_drop_output(ctx, out);
		fz_free(ctx, wri);
		fz_rethrow(ctx);
	}

	return (fz_document_writer*)wri;
}

fz_document_writer *
fz_new_text_writer(fz_context *ctx, const char *format, const char *path, const char *options)
{
	fz_output *out = fz_new_output_with_path(ctx, path ? path : "out.txt", 0);
	return fz_new_text_writer_with_output(ctx, format, out, options);
}
