#include "fitz.h"
#include "mupdf.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H

/*
 * ToUnicode map for fonts
 */

fz_error
pdf_loadtounicode(pdf_fontdesc *font, pdf_xref *xref,
	char **strings, char *collection, fz_obj *cmapstm)
{
	fz_error error = fz_okay;
	pdf_cmap *cmap;
	int cid;
	int ucs;
	int i;

	if (fz_isindirect(cmapstm))
	{
		pdf_logfont("tounicode embedded cmap\n");

		error = pdf_loadembeddedcmap(&cmap, xref, cmapstm);
		if (error)
			return fz_rethrow(error, "cannot load embedded cmap");

		error = pdf_newcmap(&font->tounicode);
		if (error)
		{
			pdf_dropcmap(cmap);
			return fz_rethrow(error, "cannot create tounicode cmap");
		}

		for (i = 0; i < (strings ? 256 : 65536); i++)
		{
			cid = pdf_lookupcmap(font->encoding, i);
			if (cid > 0)
			{
				ucs = pdf_lookupcmap(cmap, i);
				if (ucs > 0)
				{
					error = pdf_maprangetorange(font->tounicode, cid, cid, ucs);
					if (error)
					{
						pdf_dropcmap(cmap);
						return fz_rethrow(error, "cannot create tounicode mapping");
					}
				}
			}
		}

		error = pdf_sortcmap(font->tounicode);
		if (error)
		{
			pdf_dropcmap(cmap);
			return fz_rethrow(error, "cannot sort tounicode mappings");
		}

		pdf_dropcmap(cmap);
		return fz_okay;
	}

	else if (collection)
	{
		pdf_logfont("tounicode cid collection (%s)\n", collection);

		error = fz_okay;

		if (!strcmp(collection, "Adobe-CNS1"))
			error = pdf_loadsystemcmap(&font->tounicode, "Adobe-CNS1-UCS2");
		else if (!strcmp(collection, "Adobe-GB1"))
			error = pdf_loadsystemcmap(&font->tounicode, "Adobe-GB1-UCS2");
		else if (!strcmp(collection, "Adobe-Japan1"))
			error = pdf_loadsystemcmap(&font->tounicode, "Adobe-Japan1-UCS2");
		else if (!strcmp(collection, "Adobe-Japan2"))
			error = pdf_loadsystemcmap(&font->tounicode, "Adobe-Japan2-UCS2"); /* where's this? */
		else if (!strcmp(collection, "Adobe-Korea1"))
			error = pdf_loadsystemcmap(&font->tounicode, "Adobe-Korea1-UCS2");

		if (error)
			return fz_rethrow(error, "cannot load tounicode system cmap %s-UCS2", collection);
	}

	if (strings)
	{
		pdf_logfont("tounicode strings\n");

		/* TODO use tounicode cmap here ... for one-to-many mappings */

		font->ncidtoucs = 256;
		font->cidtoucs = fz_malloc(256 * sizeof(unsigned short));
		if (!font->cidtoucs)
			return fz_rethrow(-1, "out of memory: tounicode cidtoucs table");

		for (i = 0; i < 256; i++)
		{
			if (strings[i])
			{
				int aglbuf[256];
				int aglnum;
				aglnum = pdf_lookupagl(strings[i], aglbuf, nelem(aglbuf));
				if (aglnum > 0)
					font->cidtoucs[i] = aglbuf[0];
				else
					font->cidtoucs[i] = '?';
			}
			else
				font->cidtoucs[i] = '?';
		}

		return fz_okay;
	}

	if (!font->tounicode && !font->cidtoucs)
	{
	    pdf_logfont("tounicode could not be loaded\n");
	    /* TODO: synthesize a ToUnicode if it's a freetype font with
	     * cmap and/or post tables or if it has glyph names. */
	}

	return fz_okay;
}

/*
 * Extract lines of text from display tree.
 *
 * This extraction needs to be rewritten for the new tree
 * architecture where glyph index and unicode characters are both stored
 * in the text objects.
 */

fz_error
pdf_newtextline(pdf_textline **linep)
{
	pdf_textline *line;
	line = *linep = fz_malloc(sizeof(pdf_textline));
	if (!line)
		return fz_rethrow(-1, "out of memory: textline struct");
	line->len = 0;
	line->cap = 0;
	line->text = nil;
	line->next = nil;
	return fz_okay;
}

void
pdf_droptextline(pdf_textline *line)
{
	if (line->next)
		pdf_droptextline(line->next);
	fz_free(line->text);
	fz_free(line);
}

static fz_error
addtextchar(pdf_textline *line, int x, int y, int c)
{
	pdf_textchar *newtext;
	int newcap;

	if (line->len + 1 >= line->cap)
	{
		newcap = line->cap ? line->cap * 2 : 80;
		newtext = fz_realloc(line->text, sizeof(pdf_textchar) * newcap);
		if (!newtext)
			return fz_rethrow(-1, "out of memory: textline buffer resize");
		line->cap = newcap;
		line->text = newtext;
	}

	line->text[line->len].x = x;
	line->text[line->len].y = y;
	line->text[line->len].c = c;
	line->len ++;

	return fz_okay;
}

static fz_error
extracttext(pdf_textline **line, fz_node *node, fz_matrix ctm, fz_point *oldpt)
{
	fz_error error;

	if (fz_istextnode(node))
	{
		fz_textnode *text = (fz_textnode*)node;
		fz_font *font = text->font;
		fz_matrix tm = text->trm;
		fz_matrix inv = fz_invertmatrix(text->trm);
		fz_matrix trm;
		float dx, dy;
		fz_point p;
		float adv;
		int i, x, y;

		FT_Set_Transform(font->ftface, NULL, NULL);
		FT_Set_Char_Size(font->ftface, 64, 64, 72, 72);

		for (i = 0; i < text->len; i++)
		{
			tm.e = text->els[i].x;
			tm.f = text->els[i].y;
			trm = fz_concat(tm, ctm);
			x = trm.e;
			y = trm.f;
			trm.e = 0;
			trm.f = 0;

			p.x = text->els[i].x;
			p.y = text->els[i].y;
			p = fz_transformpoint(inv, p);
			dx = oldpt->x - p.x;
			dy = oldpt->y - p.y;
			*oldpt = p;

			/* TODO: flip advance and test for vertical writing */

			if (font->ftface)
			{
				FT_Fixed ftadv;
				FT_Get_Advance(font->ftface, text->els[i].gid,
					FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING,
					&ftadv);
				adv = ftadv / 65536.0;
				oldpt->x += adv;
			}
			else
			{
				adv = font->t3widths[text->els[i].gid];
				oldpt->x += adv;
			}

			if (fabs(dy) > 0.2)
			{
				pdf_textline *newline;
				error = pdf_newtextline(&newline);
				if (error)
					return fz_rethrow(error, "cannot create new text line");
				(*line)->next = newline;
				*line = newline;
			}
			else if (fabs(dx) > 0.2)
			{
				error = addtextchar(*line, x, y, ' ');
				if (error)
					return fz_rethrow(error, "cannot add character to text line");
			}

			error = addtextchar(*line, x, y, text->els[i].ucs);
			if (error)
				return fz_rethrow(error, "cannot add character to text line");
		}
	}

	if (fz_istransformnode(node))
		ctm = fz_concat(((fz_transformnode*)node)->m, ctm);

	for (node = node->first; node; node = node->next)
	{
		error = extracttext(line, node, ctm, oldpt);
		if (error)
			return fz_rethrow(error, "cannot extract text from display node");
	}

	return fz_okay;
}

fz_error
pdf_loadtextfromtree(pdf_textline **outp, fz_tree *tree, fz_matrix ctm)
{
	pdf_textline *root;
	pdf_textline *line;
	fz_error error;
	fz_point oldpt;

	oldpt.x = -1;
	oldpt.y = -1;

	error = pdf_newtextline(&root);
	if (error)
		return fz_rethrow(error, "cannot create new text line");

	line = root;

	error = extracttext(&line, tree->root, ctm, &oldpt);
	if (error)
	{
		pdf_droptextline(root);
		return fz_rethrow(error, "cannot extract text from display tree");
	}

	*outp = root;
	return fz_okay;
}

void
pdf_debugtextline(pdf_textline *line)
{
	char buf[10];
	int c, n, k, i;

	for (i = 0; i < line->len; i++)
	{
		c = line->text[i].c;
		if (c < 128)
			putchar(c);
		else
		{
			n = runetochar(buf, &c);
			for (k = 0; k < n; k++)
				putchar(buf[k]);
		}
	}
	putchar('\n');

	if (line->next)
		pdf_debugtextline(line->next);
}

