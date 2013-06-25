#include "mupdf/fitz.h"

static inline int fz_tolower(int c)
{
	/* TODO: proper unicode case folding */
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	return c;
}

static inline int iswhite(int c)
{
	return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

fz_char_and_box *fz_text_char_at(fz_char_and_box *cab, fz_text_page *page, int idx)
{
	int block_num;
	int ofs = 0;

	for (block_num = 0; block_num < page->len; block_num++)
	{
		fz_text_block *block;
		fz_text_line *line;
		fz_text_span *span;

		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->first_span; span; span = span->next)
			{
				if (idx < ofs + span->len)
				{
					cab->c = span->text[idx - ofs].c;
					fz_text_char_bbox(&cab->bbox, span, idx - ofs);
					return cab;
				}
				ofs += span->len;
			}
			/* pseudo-newline */
			if (idx == ofs)
			{
				cab->bbox = fz_empty_rect;
				cab->c = ' ';
				return cab;
			}
			ofs++;
		}
	}
	cab->bbox = fz_empty_rect;
	cab->c = 0;
	return cab;
}

static int charat(fz_text_page *page, int idx)
{
	fz_char_and_box cab;
	return fz_text_char_at(&cab, page, idx)->c;
}

static fz_rect *bboxat(fz_text_page *page, int idx, fz_rect *bbox)
{
	fz_char_and_box cab;
	/* FIXME: Nasty extra copy */
	*bbox = fz_text_char_at(&cab, page, idx)->bbox;
	return bbox;
}

static int textlen(fz_text_page *page)
{
	int len = 0;
	int block_num;

	for (block_num = 0; block_num < page->len; block_num++)
	{
		fz_text_block *block;
		fz_text_line *line;
		fz_text_span *span;

		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->first_span; span; span = span->next)
			{
				len += span->len;
			}
			len++; /* pseudo-newline */
		}
	}
	return len;
}

static int match(fz_text_page *page, const char *s, int n)
{
	int orig = n;
	int c;
	while (*s)
	{
		s += fz_chartorune(&c, (char *)s);
		if (iswhite(c) && iswhite(charat(page, n)))
		{
			const char *s_next;

			/* Skip over whitespace in the document */
			do
				n++;
			while (iswhite(charat(page, n)));

			/* Skip over multiple whitespace in the search string */
			while (s_next = s + fz_chartorune(&c, (char *)s), iswhite(c))
				s = s_next;
		}
		else
		{
			if (fz_tolower(c) != fz_tolower(charat(page, n)))
				return 0;
			n++;
		}
	}
	return n - orig;
}

int
fz_search_text_page(fz_context *ctx, fz_text_page *text, const char *needle, fz_rect *hit_bbox, int hit_max)
{
	int pos, len, i, n, hit_count;

	if (strlen(needle) == 0)
		return 0;

	hit_count = 0;
	len = textlen(text);
	for (pos = 0; pos < len; pos++)
	{
		n = match(text, needle, pos);
		if (n)
		{
			fz_rect linebox = fz_empty_rect;
			for (i = 0; i < n; i++)
			{
				fz_rect charbox;
				bboxat(text, pos + i, &charbox);
				if (!fz_is_empty_rect(&charbox))
				{
					if (charbox.y0 != linebox.y0 || fz_abs(charbox.x0 - linebox.x1) > 5)
					{
						if (!fz_is_empty_rect(&linebox) && hit_count < hit_max)
							hit_bbox[hit_count++] = linebox;
						linebox = charbox;
					}
					else
					{
						fz_union_rect(&linebox, &charbox);
					}
				}
			}
			if (!fz_is_empty_rect(&linebox) && hit_count < hit_max)
				hit_bbox[hit_count++] = linebox;
		}
	}

	return hit_count;
}

int
fz_highlight_selection(fz_context *ctx, fz_text_page *page, fz_rect rect, fz_rect *hit_bbox, int hit_max)
{
	fz_rect linebox, charbox;
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	int i, block_num, hit_count;

	float x0 = rect.x0;
	float x1 = rect.x1;
	float y0 = rect.y0;
	float y1 = rect.y1;

	hit_count = 0;

	for (block_num = 0; block_num < page->len; block_num++)
	{
		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			linebox = fz_empty_rect;
			for (span = line->first_span; span; span = span->next)
			{
				for (i = 0; i < span->len; i++)
				{
					fz_text_char_bbox(&charbox, span, i);
					if (charbox.x1 >= x0 && charbox.x0 <= x1 && charbox.y1 >= y0 && charbox.y0 <= y1)
					{
						if (charbox.y0 != linebox.y0 || fz_abs(charbox.x0 - linebox.x1) > 5)
						{
							if (!fz_is_empty_rect(&linebox) && hit_count < hit_max)
								hit_bbox[hit_count++] = linebox;
							linebox = charbox;
						}
						else
						{
							fz_union_rect(&linebox, &charbox);
						}
					}
				}
			}
			if (!fz_is_empty_rect(&linebox) && hit_count < hit_max)
				hit_bbox[hit_count++] = linebox;
		}
	}

	return hit_count;
}

char *
fz_copy_selection(fz_context *ctx, fz_text_page *page, fz_rect rect)
{
	fz_buffer *buffer;
	fz_rect hitbox;
	int c, i, block_num, seen = 0;
	char *s;

	float x0 = rect.x0;
	float x1 = rect.x1;
	float y0 = rect.y0;
	float y1 = rect.y1;

	buffer = fz_new_buffer(ctx, 1024);

	for (block_num = 0; block_num < page->len; block_num++)
	{
		fz_text_block *block;
		fz_text_line *line;
		fz_text_span *span;

		if (page->blocks[block_num].type != FZ_PAGE_BLOCK_TEXT)
			continue;
		block = page->blocks[block_num].u.text;
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->first_span; span; span = span->next)
			{
				if (seen)
				{
					fz_write_buffer_byte(ctx, buffer, '\n');
				}

				seen = 0;

				for (i = 0; i < span->len; i++)
				{
					fz_text_char_bbox(&hitbox, span, i);
					c = span->text[i].c;
					if (c < 32)
						c = '?';
					if (hitbox.x1 >= x0 && hitbox.x0 <= x1 && hitbox.y1 >= y0 && hitbox.y0 <= y1)
					{
						fz_write_buffer_rune(ctx, buffer, c);
						seen = 1;
					}
				}

				seen = (seen && span == line->last_span);
			}
		}
	}

	fz_write_buffer_byte(ctx, buffer, 0);

	s = (char*)buffer->data;
	fz_free(ctx, buffer);
	return s;
}
