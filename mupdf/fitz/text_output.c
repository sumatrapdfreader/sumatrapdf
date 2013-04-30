#include "fitz-internal.h"

#define SUBSCRIPT_OFFSET 0.2f
#define SUPERSCRIPT_OFFSET -0.2f

#include <ft2build.h>
#include FT_FREETYPE_H

/* XML, HTML and plain-text output */

static int font_is_bold(fz_font *font)
{
	FT_Face face = font->ft_face;
	if (face && (face->style_flags & FT_STYLE_FLAG_BOLD))
		return 1;
	if (strstr(font->name, "Bold"))
		return 1;
	return 0;
}

static int font_is_italic(fz_font *font)
{
	FT_Face face = font->ft_face;
	if (face && (face->style_flags & FT_STYLE_FLAG_ITALIC))
		return 1;
	if (strstr(font->name, "Italic") || strstr(font->name, "Oblique"))
		return 1;
	return 0;
}

static void
fz_print_style_begin(fz_output *out, fz_text_style *style)
{
	int script = style->script;
	fz_printf(out, "<span class=\"s%d\">", style->id);
	while (script-- > 0)
		fz_printf(out, "<sup>");
	while (++script < 0)
		fz_printf(out, "<sub>");
}

static void
fz_print_style_end(fz_output *out, fz_text_style *style)
{
	int script = style->script;
	while (script-- > 0)
		fz_printf(out, "</sup>");
	while (++script < 0)
		fz_printf(out, "</sub>");
	fz_printf(out, "</span>");
}

static void
fz_print_style(fz_output *out, fz_text_style *style)
{
	char *s = strchr(style->font->name, '+');
	s = s ? s + 1 : style->font->name;
	fz_printf(out, "span.s%d{font-family:\"%s\";font-size:%gpt;",
		style->id, s, style->size);
	if (font_is_italic(style->font))
		fz_printf(out, "font-style:italic;");
	if (font_is_bold(style->font))
		fz_printf(out, "font-weight:bold;");
	fz_printf(out, "}\n");
}

void
fz_print_text_sheet(fz_context *ctx, fz_output *out, fz_text_sheet *sheet)
{
	fz_text_style *style;
	for (style = sheet->style; style; style = style->next)
		fz_print_style(out, style);
}

static void
send_data_base64(fz_output *out, fz_buffer *buffer)
{
	int i, len;
	static const char set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	len = buffer->len/3;
	for (i = 0; i < len; i++)
	{
		int c = buffer->data[3*i];
		int d = buffer->data[3*i+1];
		int e = buffer->data[3*i+2];
		if ((i & 15) == 0)
			fz_printf(out, "\n");
		fz_printf(out, "%c%c%c%c", set[c>>2], set[((c&3)<<4)|(d>>4)], set[((d&15)<<2)|(e>>6)], set[e & 63]);
	}
	i *= 3;
	switch (buffer->len-i)
	{
		case 2:
		{
			int c = buffer->data[i];
			int d = buffer->data[i+1];
			fz_printf(out, "%c%c%c=", set[c>>2], set[((c&3)<<4)|(d>>4)], set[((d&15)<<2)]);
			break;
		}
	case 1:
		{
			int c = buffer->data[i];
			fz_printf(out, "%c%c==", set[c>>2], set[(c&3)<<4]);
			break;
		}
	default:
	case 0:
		break;
	}
}

void
fz_print_text_page_html(fz_context *ctx, fz_output *out, fz_text_page *page)
{
	int block_n, line_n, span_n, ch_n;
	fz_text_style *style = NULL;
	fz_text_line *line;
	void *last_region = NULL;

	fz_printf(out, "<div class=\"page\">\n");

	for (block_n = 0; block_n < page->len; block_n++)
	{
		switch(page->blocks[block_n].type)
		{
		case FZ_PAGE_BLOCK_TEXT:
		{
			fz_text_block * block = page->blocks[block_n].u.text;
			fz_printf(out, "<div class=\"block\"><p>\n");
			for (line_n = 0; line_n < block->len; line_n++)
			{
				int lastcol=-1;
				line = &block->lines[line_n];
				style = NULL;

				if (line->region != last_region)
				{
					if (last_region)
						fz_printf(out, "</div>");
					fz_printf(out, "<div class=\"metaline\">");
					last_region = line->region;
				}
				fz_printf(out, "<div class=\"line\"");
#ifdef DEBUG_INTERNALS
				if (line->region)
					fz_printf(out, " region=\"%x\"", line->region);
#endif
				fz_printf(out, ">");
				for (span_n = 0; span_n < line->len; span_n++)
				{
					fz_text_span *span = line->spans[span_n];
					float size = fz_matrix_expansion(&span->transform);
					float base_offset = span->base_offset / size;

					if (lastcol != span->column)
					{
						if (lastcol >= 0)
						{
							fz_printf(out, "</div>");
						}
						/* If we skipped any columns then output some spacer spans */
						while (lastcol < span->column-1)
						{
							fz_printf(out, "<div class=\"cell\"></div>");
							lastcol++;
						}
						lastcol++;
						/* Now output the span to contain this entire column */
						fz_printf(out, "<div class=\"cell\" style=\"");
						{
							int sn;
							for (sn = span_n+1; sn < line->len; sn++)
							{
								if (line->spans[sn]->column != lastcol)
									break;
							}
							fz_printf(out, "width:%g%%;align:%s", span->column_width, (span->align == 0 ? "left" : (span->align == 1 ? "center" : "right")));
						}
						if (span->indent > 1)
							fz_printf(out, ";padding-left:1em;text-indent:-1em");
						if (span->indent < -1)
							fz_printf(out, ";text-indent:1em");
						fz_printf(out, "\">");
					}
#ifdef DEBUG_INTERNALS
					fz_printf(out, "<span class=\"internal_span\"");
					if (span->column)
						fz_printf(out, " col=\"%x\"", span->column);
					fz_printf(out, ">");
#endif
					if (span->spacing >= 1)
						fz_printf(out, " ");
					if (base_offset > SUBSCRIPT_OFFSET)
						fz_printf(out, "<sub>");
					else if (base_offset < SUPERSCRIPT_OFFSET)
						fz_printf(out, "<sup>");
					for (ch_n = 0; ch_n < span->len; ch_n++)
					{
						fz_text_char *ch = &span->text[ch_n];
						if (style != ch->style)
						{
							if (style)
								fz_print_style_end(out, style);
							fz_print_style_begin(out, ch->style);
							style = ch->style;
						}

						if (ch->c == '<')
							fz_printf(out, "&lt;");
						else if (ch->c == '>')
							fz_printf(out, "&gt;");
						else if (ch->c == '&')
							fz_printf(out, "&amp;");
						else if (ch->c >= 32 && ch->c <= 127)
							fz_printf(out, "%c", ch->c);
						else
							fz_printf(out, "&#x%x;", ch->c);
					}
					if (style)
					{
						fz_print_style_end(out, style);
						style = NULL;
					}
					if (base_offset > SUBSCRIPT_OFFSET)
						fz_printf(out, "</sub>");
					else if (base_offset < SUPERSCRIPT_OFFSET)
						fz_printf(out, "</sup>");
#ifdef DEBUG_INTERNALS
					fz_printf(out, "</span>");
#endif
				}
				/* Close our floating span */
				fz_printf(out, "</div>");
				/* Close the line */
				fz_printf(out, "</div>");
				fz_printf(out, "\n");
			}
			/* Close the metaline */
			fz_printf(out, "</div>");
			last_region = NULL;
			fz_printf(out, "</p></div>\n");
			break;
		}
		case FZ_PAGE_BLOCK_IMAGE:
		{
			fz_image_block *image = page->blocks[block_n].u.image;
			fz_printf(out, "<img width=%d height=%d src=\"data:", image->image->w, image->image->h);
			switch (image->image->buffer == NULL ? FZ_IMAGE_JPX : image->image->buffer->params.type)
			{
			case FZ_IMAGE_JPEG:
				fz_printf(out, "image/jpeg;base64,");
				send_data_base64(out, image->image->buffer->buffer);
				break;
			case FZ_IMAGE_PNG:
				fz_printf(out, "image/png;base64,");
				send_data_base64(out, image->image->buffer->buffer);
				break;
			default:
				{
					fz_pixmap *pix = fz_image_get_pixmap(ctx, image->image, image->image->w, image->image->h);
					fz_buffer *buf = fz_new_buffer(ctx, 1024);
					fz_output *out2 = fz_new_output_with_buffer(ctx, buf);
					fz_output_pixmap_to_png(ctx, pix, out2, 0);
					fz_close_output(out2);
					fz_printf(out, "image/png;base64,");
					send_data_base64(out, buf);
					fz_drop_buffer(ctx, buf);
					fz_drop_pixmap(ctx, pix);
					break;
				}
			}
			fz_printf(out, "\">\n");
			break;
		}
		}
	}

	fz_printf(out, "</div>\n");
}

void
fz_print_text_page_xml(fz_context *ctx, fz_output *out, fz_text_page *page)
{
	int block_n;

	fz_printf(out, "<page>\n");
	for (block_n = 0; block_n < page->len; block_n++)
	{
		switch(page->blocks[block_n].type)
		{
		case FZ_PAGE_BLOCK_TEXT:
		{
			fz_text_block *block = page->blocks[block_n].u.text;
			fz_text_line *line;
			char *s;

			fz_printf(out, "<block bbox=\"%g %g %g %g\">\n",
				block->bbox.x0, block->bbox.y0, block->bbox.x1, block->bbox.y1);
			for (line = block->lines; line < block->lines + block->len; line++)
			{
				int span_num;
				fz_printf(out, "<line bbox=\"%g %g %g %g\">\n",
					line->bbox.x0, line->bbox.y0, line->bbox.x1, line->bbox.y1);
				for (span_num = 0; span_num < line->len; span_num++)
				{
					fz_text_span *span = line->spans[span_num];
					fz_text_style *style = NULL;
					int char_num;
					for (char_num = 0; char_num < span->len; char_num++)
					{
						fz_text_char *ch = &span->text[char_num];
						if (ch->style != style)
						{
							if (style)
							{
								fz_printf(out, "</span>\n");
							}
							style = ch->style;
							s = strchr(style->font->name, '+');
							s = s ? s + 1 : style->font->name;
							fz_printf(out, "<span bbox=\"%g %g %g %g\" font=\"%s\" size=\"%g\">\n",
								span->bbox.x0, span->bbox.y0, span->bbox.x1, span->bbox.y1,
								s, style->size);
						}
						{
							fz_rect rect;
							fz_text_char_bbox(&rect, span, char_num);
							fz_printf(out, "<char bbox=\"%g %g %g %g\" x=\"%g\" y=\"%g\" c=\"",
								rect.x0, rect.y0, rect.x1, rect.y1, ch->p.x, ch->p.y);
						}
						switch (ch->c)
						{
						case '<': fz_printf(out, "&lt;"); break;
						case '>': fz_printf(out, "&gt;"); break;
						case '&': fz_printf(out, "&amp;"); break;
						case '"': fz_printf(out, "&quot;"); break;
						case '\'': fz_printf(out, "&apos;"); break;
						default:
							if (ch->c >= 32 && ch->c <= 127)
								fz_printf(out, "%c", ch->c);
							else
								fz_printf(out, "&#x%x;", ch->c);
							break;
						}
						fz_printf(out, "\"/>\n");
					}
					if (style)
						fz_printf(out, "</span>\n");
				}
				fz_printf(out, "</line>\n");
			}
			fz_printf(out, "</block>\n");
			break;
		}
		case FZ_PAGE_BLOCK_IMAGE:
		{
			break;
		}
	}
	}
	fz_printf(out, "</page>\n");
}

void
fz_print_text_page(fz_context *ctx, fz_output *out, fz_text_page *page)
{
	int block_n;

	for (block_n = 0; block_n < page->len; block_n++)
	{
		switch(page->blocks[block_n].type)
		{
		case FZ_PAGE_BLOCK_TEXT:
		{
			fz_text_block *block = page->blocks[block_n].u.text;
			fz_text_line *line;
			fz_text_char *ch;
			char utf[10];
			int i, n;

			for (line = block->lines; line < block->lines + block->len; line++)
			{
				int span_num;
				for (span_num = 0; span_num < line->len; span_num++)
				{
					fz_text_span *span = line->spans[span_num];
					for (ch = span->text; ch < span->text + span->len; ch++)
					{
						n = fz_runetochar(utf, ch->c);
						for (i = 0; i < n; i++)
							fz_printf(out, "%c", utf[i]);
					}
					/* SumatraPDF: separate spans with spaces */
					if (span_num < line->len - 1 && span->len > 0 && span->text[span->len - 1].c != ' ')
						fz_printf(out, " ");
				}
				fz_printf(out, "\n");
			}
			fz_printf(out, "\n");
			break;
		}
		case FZ_PAGE_BLOCK_IMAGE:
			break;
		}
	}
}
