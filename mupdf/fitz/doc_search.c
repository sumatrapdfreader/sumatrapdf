#include "fitz-internal.h"

static inline int fz_tolower(int c)
{
	/* TODO: proper unicode case folding */
	if (c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	return c;
}

static fz_text_char textcharat(fz_text_page *page, int idx)
{
	static fz_text_char emptychar = { {0,0,0,0}, ' ' };
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	int ofs = 0;
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->spans; span < line->spans + line->len; span++)
			{
				if (idx < ofs + span->len)
					return span->text[idx - ofs];
				/* pseudo-newline */
				if (span + 1 == line->spans + line->len)
				{
					if (idx == ofs + span->len)
						return emptychar;
					ofs++;
				}
				ofs += span->len;
			}
		}
	}
	return emptychar;
}

static int charat(fz_text_page *page, int idx)
{
	return textcharat(page, idx).c;
}

static fz_bbox bboxat(fz_text_page *page, int idx)
{
	return fz_round_rect(textcharat(page, idx).bbox);
}

static int textlen(fz_text_page *page)
{
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	int len = 0;
	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->spans; span < line->spans + line->len; span++)
				len += span->len;
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
		if (c == ' ' && charat(page, n) == ' ')
		{
			while (charat(page, n) == ' ')
				n++;
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
fz_search_text_page(fz_context *ctx, fz_text_page *text, char *needle, fz_bbox *hit_bbox, int hit_max)
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
			fz_bbox linebox = fz_empty_bbox;
			for (i = 0; i < n; i++)
			{
				fz_bbox charbox = bboxat(text, pos + i);
				if (!fz_is_empty_bbox(charbox))
				{
					if (charbox.y0 != linebox.y0 || fz_absi(charbox.x0 - linebox.x1) > 5)
					{
						if (!fz_is_empty_bbox(linebox) && hit_count < hit_max)
							hit_bbox[hit_count++] = linebox;
						linebox = charbox;
					}
					else
					{
						linebox = fz_union_bbox(linebox, charbox);
					}
				}
			}
			if (!fz_is_empty_bbox(linebox) && hit_count < hit_max)
				hit_bbox[hit_count++] = linebox;
		}
	}

	return hit_count;
}

int
fz_highlight_selection(fz_context *ctx, fz_text_page *page, fz_bbox rect, fz_bbox *hit_bbox, int hit_max)
{
	fz_bbox linebox, charbox;
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	int i, hit_count;

	int x0 = rect.x0;
	int x1 = rect.x1;
	int y0 = rect.y0;
	int y1 = rect.y1;

	hit_count = 0;

	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			linebox = fz_empty_bbox;
			for (span = line->spans; span < line->spans + line->len; span++)
			{
				for (i = 0; i < span->len; i++)
				{
					charbox = fz_bbox_covering_rect(span->text[i].bbox);
					if (charbox.x1 >= x0 && charbox.x0 <= x1 && charbox.y1 >= y0 && charbox.y0 <= y1)
					{
						if (charbox.y0 != linebox.y0 || fz_absi(charbox.x0 - linebox.x1) > 5)
						{
							if (!fz_is_empty_bbox(linebox) && hit_count < hit_max)
								hit_bbox[hit_count++] = linebox;
							linebox = charbox;
						}
						else
						{
							linebox = fz_union_bbox(linebox, charbox);
						}
					}
				}
			}
			if (!fz_is_empty_bbox(linebox) && hit_count < hit_max)
				hit_bbox[hit_count++] = linebox;
		}
	}

	return hit_count;
}

char *
fz_copy_selection(fz_context *ctx, fz_text_page *page, fz_bbox rect)
{
	fz_buffer *buffer;
	fz_bbox hitbox;
	fz_text_block *block;
	fz_text_line *line;
	fz_text_span *span;
	int c, i, seen = 0;
	char *s;

	int x0 = rect.x0;
	int x1 = rect.x1;
	int y0 = rect.y0;
	int y1 = rect.y1;

	buffer = fz_new_buffer(ctx, 1024);

	for (block = page->blocks; block < page->blocks + page->len; block++)
	{
		for (line = block->lines; line < block->lines + block->len; line++)
		{
			for (span = line->spans; span < line->spans + line->len; span++)
			{
				if (seen)
				{
					fz_write_buffer_byte(ctx, buffer, '\n');
				}

				seen = 0;

				for (i = 0; i < span->len; i++)
				{
					hitbox = fz_bbox_covering_rect(span->text[i].bbox);
					c = span->text[i].c;
					if (c < 32)
						c = '?';
					if (hitbox.x1 >= x0 && hitbox.x0 <= x1 && hitbox.y1 >= y0 && hitbox.y0 <= y1)
					{
						fz_write_buffer_rune(ctx, buffer, c);
						seen = 1;
					}
				}

				seen = (seen && span + 1 == line->spans + line->len);
			}
		}
	}

	fz_write_buffer_byte(ctx, buffer, 0);

	s = (char*)buffer->data;
	fz_free(ctx, buffer);
	return s;
}
