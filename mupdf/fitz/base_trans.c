#include "fitz-internal.h"

static int
fade(fz_pixmap *tpix, fz_pixmap *opix, fz_pixmap *npix, int time)
{
	unsigned char *t, *o, *n;
	int size;

	if (!tpix || !opix || !npix || tpix->w != opix->w || opix->w != npix->w || tpix->h != opix->h || opix->h != npix->h || tpix->n != opix->n || opix->n != npix->n)
		return 0;
	size = tpix->w * tpix->h * tpix->n;
	t = tpix->samples;
	o = opix->samples;
	n = npix->samples;
	while (size-- > 0)
	{
		int op = *o++;
		int np = *n++;
		*t++ = ((op<<8) + ((np-op) * time) + 0x80)>>8;
	}
	return 1;
}

static int
blind_horiz(fz_pixmap *tpix, fz_pixmap *opix, fz_pixmap *npix, int time)
{
	unsigned char *t, *o, *n;
	int blind_height, size, span, position, y;

	if (!tpix || !opix || !npix || tpix->w != opix->w || opix->w != npix->w || tpix->h != opix->h || opix->h != npix->h || tpix->n != opix->n || opix->n != npix->n)
		return 0;
	span = tpix->w * tpix->n;
	size = tpix->h * span;
	blind_height = (tpix->h+7) / 8;
	position = blind_height * time / 256;
	t = tpix->samples;
	o = opix->samples;
	n = npix->samples;
	for (y = 0; y < tpix->h; y++)
	{
		memcpy(t, ((y % blind_height) <= position ? n : o), span);
		t += span;
		o += span;
		n += span;
	}
	return 1;
}

static int
blind_vertical(fz_pixmap *tpix, fz_pixmap *opix, fz_pixmap *npix, int time)
{
	unsigned char *t, *o, *n;
	int blind_width, size, span, position, y;

	if (!tpix || !opix || !npix || tpix->w != opix->w || opix->w != npix->w || tpix->h != opix->h || opix->h != npix->h || tpix->n != opix->n || opix->n != npix->n)
		return 0;
	span = tpix->w * tpix->n;
	size = tpix->h * span;
	blind_width = (tpix->w+7) / 8;
	position = blind_width * time / 256;
	blind_width *= tpix->n;
	position *= tpix->n;
	t = tpix->samples;
	o = opix->samples;
	n = npix->samples;
	for (y = 0; y < tpix->h; y++)
	{
		int w, x;
		x = 0;
		while ((w = span - x) > 0)
		{
			int p;
			if (w > blind_width)
				w = blind_width;
			p = position;
			if (p > w)
				p = w;
			memcpy(t, n, p);
			memcpy(t+position, o+position, w - p);
			x += blind_width;
			t += w;
			o += w;
			n += w;
		}
	}
	return 1;
}

static int
wipe_tb(fz_pixmap *tpix, fz_pixmap *opix, fz_pixmap *npix, int time)
{
	unsigned char *t, *o, *n;
	int span, position, y;

	if (!tpix || !opix || !npix || tpix->w != opix->w || opix->w != npix->w || tpix->h != opix->h || opix->h != npix->h || tpix->n != opix->n || opix->n != npix->n)
		return 0;
	span = tpix->w * tpix->n;
	position = tpix->h * time / 256;
	t = tpix->samples;
	o = opix->samples;
	n = npix->samples;
	for (y = 0; y < position; y++)
	{
		memcpy(t, n, span);
		t += span;
		o += span;
		n += span;
	}
	for (; y < tpix->h; y++)
	{
		memcpy(t, o, span);
		t += span;
		o += span;
		n += span;
	}
	return 1;
}

static int
wipe_lr(fz_pixmap *tpix, fz_pixmap *opix, fz_pixmap *npix, int time)
{
	unsigned char *t, *o, *n;
	int span, position, y;

	if (!tpix || !opix || !npix || tpix->w != opix->w || opix->w != npix->w || tpix->h != opix->h || opix->h != npix->h || tpix->n != opix->n || opix->n != npix->n)
		return 0;
	span = tpix->w * tpix->n;
	position = tpix->w * time / 256;
	position *= tpix->n;
	t = tpix->samples;
	o = opix->samples + position;
	n = npix->samples;
	for (y = 0; y < tpix->h; y++)
	{
		memcpy(t, n, position);
		memcpy(t+position, o, span-position);
		t += span;
		o += span;
		n += span;
	}
	return 1;
}

int fz_generate_transition(fz_pixmap *tpix, fz_pixmap *opix, fz_pixmap *npix, int time, fz_transition *trans)
{
	switch (trans->type)
	{
	default:
	case FZ_TRANSITION_FADE:
		return fade(tpix, opix, npix, time);
	case FZ_TRANSITION_BLINDS:
		if (trans->vertical)
			return blind_vertical(tpix, opix, npix, time);
		else
			return blind_horiz(tpix, opix, npix, time);
	case FZ_TRANSITION_WIPE:
		switch (((trans->direction + 45 + 360) % 360) / 90)
		{
		default:
		case 0: return wipe_lr(tpix, opix, npix, time);
		case 1: return wipe_tb(tpix, npix, opix, 256-time);
		case 2: return wipe_lr(tpix, npix, opix, 256-time);
		case 3: return wipe_tb(tpix, opix, npix, time);
		}
	}
	return 0;
}
