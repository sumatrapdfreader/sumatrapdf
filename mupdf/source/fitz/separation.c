#include "mupdf/fitz.h"
#include "fitz-imp.h"

#include <assert.h>
#include <string.h>

enum
{
	FZ_SEPARATION_DISABLED_RENDER = 3
};

struct fz_separations_s
{
	int refs;
	int num_separations;
	int controllable;
	uint32_t state[(2*FZ_MAX_SEPARATIONS + 31) / 32];
	fz_colorspace *cs[FZ_MAX_SEPARATIONS];
	uint8_t cs_pos[FZ_MAX_SEPARATIONS];
	uint32_t rgba[FZ_MAX_SEPARATIONS];
	uint32_t cmyk[FZ_MAX_SEPARATIONS];
	char *name[FZ_MAX_SEPARATIONS];
};

/* Create a new separations structure (initially empty) */
fz_separations *fz_new_separations(fz_context *ctx, int controllable)
{
	fz_separations *sep;

	sep = fz_malloc_struct(ctx, fz_separations);
	sep->refs = 1;
	sep->controllable = controllable;

	return sep;
}

fz_separations *fz_keep_separations(fz_context *ctx, fz_separations *sep)
{
	return fz_keep_imp(ctx, sep, &sep->refs);
}

void fz_drop_separations(fz_context *ctx, fz_separations *sep)
{
	if (fz_drop_imp(ctx, sep, &sep->refs))
	{
		int i;
		for (i = 0; i < sep->num_separations; i++)
		{
			fz_free(ctx, sep->name[i]);
			fz_drop_colorspace(ctx, sep->cs[i]);
		}
		fz_free(ctx, sep);
	}
}

/* Add a separation (null terminated name, colorspace) */
void fz_add_separation(fz_context *ctx, fz_separations *sep, const char *name, fz_colorspace *cs, int colorant)
{
	int n;

	if (!sep)
		fz_throw(ctx, FZ_ERROR_GENERIC, "can't add to non-existent separations");

	n = sep->num_separations;
	if (n == FZ_MAX_SEPARATIONS)
		fz_throw(ctx, FZ_ERROR_GENERIC, "too many separations");

	sep->name[n] = fz_strdup(ctx, name);
	sep->cs[n] = fz_keep_colorspace(ctx, cs);
	sep->cs_pos[n] = colorant;

	sep->num_separations++;
}

/* Add a separation with equivalents (null terminated name, colorspace) (old, deprecated) */
void fz_add_separation_equivalents(fz_context *ctx, fz_separations *sep, uint32_t rgba, uint32_t cmyk, const char *name)
{
	int n;

	if (!sep)
		fz_throw(ctx, FZ_ERROR_GENERIC, "can't add to non-existent separations");

	n = sep->num_separations;
	if (n == FZ_MAX_SEPARATIONS)
		fz_throw(ctx, FZ_ERROR_GENERIC, "too many separations");

	sep->name[n] = fz_strdup(ctx, name);
	sep->rgba[n] = rgba;
	sep->cmyk[n] = cmyk;

	sep->num_separations++;
}

/* Control the rendering of a given separation */
void fz_set_separation_behavior(fz_context *ctx, fz_separations *sep, int separation, fz_separation_behavior beh)
{
	int shift;
	fz_separation_behavior old;

	if (!sep || separation < 0 || separation >= sep->num_separations)
		fz_throw(ctx, FZ_ERROR_GENERIC, "can't control non-existent separation");

	if (beh == FZ_SEPARATION_DISABLED && !sep->controllable)
		beh = FZ_SEPARATION_DISABLED_RENDER;

	shift = ((2*separation) & 31);
	separation >>= 4;

	old = (sep->state[separation]>>shift) & 3;

	if (old == (fz_separation_behavior)FZ_SEPARATION_DISABLED_RENDER)
		old = FZ_SEPARATION_DISABLED;

	/* If no change, great */
	if (old == beh)
		return;

	sep->state[separation] = (sep->state[separation] & ~(3<<shift)) | (beh<<shift);

	/* FIXME: Could only empty images from the store, or maybe only
	 * images that depend on separations. */
	fz_empty_store(ctx);
}

static inline fz_separation_behavior
sep_state(const fz_separations *sep, int i)
{
	return (fz_separation_behavior)((sep->state[i>>5]>>((2*i) & 31)) & 3);
}

fz_separation_behavior fz_separation_current_behavior_internal(fz_context *ctx, const fz_separations *sep, int separation)
{
	if (!sep || separation < 0 || separation >= sep->num_separations)
		fz_throw(ctx, FZ_ERROR_GENERIC, "can't disable non-existent separation");

	return sep_state(sep, separation);
}

/* Test for the current behavior of a separation */
fz_separation_behavior fz_separation_current_behavior(fz_context *ctx, const fz_separations *sep, int separation)
{
	int beh = fz_separation_current_behavior_internal(ctx, sep, separation);

	if (beh == FZ_SEPARATION_DISABLED_RENDER)
		return FZ_SEPARATION_DISABLED;
	return beh;
}

const char *fz_separation_name(fz_context *ctx, const fz_separations *sep, int separation)
{
	if (!sep || separation < 0 || separation >= sep->num_separations)
		fz_throw(ctx, FZ_ERROR_GENERIC, "can't access non-existent separation");

	return sep->name[separation];
}

int fz_count_separations(fz_context *ctx, const fz_separations *sep)
{
	if (!sep)
		return 0;
	return sep->num_separations;
}

/* Return the number of active separations. */
int fz_count_active_separations(fz_context *ctx, const fz_separations *sep)
{
	int i, n, c;

	if (!sep)
		return 0;
	n = sep->num_separations;
	c = 0;
	for (i = 0; i < n; i++)
		if (sep_state(sep, i) == FZ_SEPARATION_SPOT)
			c++;
	return c;
}

/* Return a separations object with all the spots in the input
 * separations object that are set to composite, reset to be
 * enabled. If there ARE no spots in the object, this returns
 * NULL. If the object already has all its spots enabled, then
 * just returns another handle on the same object. */
fz_separations *fz_clone_separations_for_overprint(fz_context *ctx, fz_separations *sep)
{
	int i, j, n, c;
	fz_separations *clone;

	if (!sep)
		return NULL;

	n = sep->num_separations;
	if (n == 0)
		return NULL;
	c = 0;
	for (i = 0; i < n; i++)
	{
		fz_separation_behavior state = sep_state(sep, i);
		if (state == FZ_SEPARATION_COMPOSITE)
			c++;
	}

	/* If no composites, then we don't need to create a new seps object
	 * with the composite ones enabled, so just reuse our current object. */
	if (c == 0)
		return fz_keep_separations(ctx, sep);

	/* We need to clone us a separation structure, with all
	 * the composite separations marked as enabled. */
	clone = fz_malloc_struct(ctx, fz_separations);
	clone->refs = 1;
	clone->controllable = 0;

	fz_try(ctx)
	{
		for (i = 0; i < n; i++)
		{
			fz_separation_behavior beh = sep_state(sep, i);
			if (beh == FZ_SEPARATION_DISABLED)
				continue;
			j = clone->num_separations++;
			if (beh == FZ_SEPARATION_COMPOSITE)
				beh = FZ_SEPARATION_SPOT;
			fz_set_separation_behavior(ctx, clone, j, beh);
			clone->name[j] = sep->name[i] ? fz_strdup(ctx, sep->name[i]) : NULL;
			clone->cs[j] = fz_keep_colorspace(ctx, sep->cs[i]);
			clone->cs_pos[j] = sep->cs_pos[i];
		}
	}
	fz_catch(ctx)
	{
		fz_drop_separations(ctx, clone);
		fz_rethrow(ctx);
	}

	return clone;
}

/*
	Convert between
	different separation results.
*/
fz_pixmap *
fz_clone_pixmap_area_with_different_seps(fz_context *ctx, fz_pixmap *src, const fz_irect *bbox, fz_colorspace *dcs, fz_separations *dseps, fz_color_params color_params, fz_default_colorspaces *default_cs)
{
	fz_irect local_bbox;
	fz_pixmap *dst, *pix;

	if (bbox == NULL)
	{
		local_bbox.x0 = src->x;
		local_bbox.y0 = src->y;
		local_bbox.x1 = src->x + src->w;
		local_bbox.y1 = src->y + src->h;
		bbox = &local_bbox;
	}

	dst = fz_new_pixmap_with_bbox(ctx, dcs, *bbox, dseps, src->alpha);
	if (src->flags & FZ_PIXMAP_FLAG_INTERPOLATE)
		dst->flags |= FZ_PIXMAP_FLAG_INTERPOLATE;
	else
		dst->flags &= ~FZ_PIXMAP_FLAG_INTERPOLATE;

	fz_try(ctx)
		pix = fz_copy_pixmap_area_converting_seps(ctx, src, dst, NULL, color_params, default_cs);
	fz_catch(ctx)
	{
		fz_drop_pixmap(ctx, dst);
		fz_rethrow(ctx);
	}

	return pix;
}

/*
	We assume that we never map from a DeviceN space to another DeviceN space here.
 */
fz_pixmap *
fz_copy_pixmap_area_converting_seps(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst, fz_colorspace *prf, fz_color_params color_params, fz_default_colorspaces *default_cs)
{
	int dw = dst->w;
	int dh = dst->h;
	fz_separations *sseps = src->seps;
	fz_separations *dseps = dst->seps;
	int sseps_n = sseps ? sseps->num_separations : 0;
	int dseps_n = dseps ? dseps->num_separations : 0;
	int sstride = src->stride;
	int dstride = dst->stride;
	int sn = src->n;
	int dn = dst->n;
	int sa = src->alpha;
	int da = dst->alpha;
	int ss = src->s;
	int ds = dst->s;
	int sc = sn - ss - sa;
	int dc = dn - ds - da;
	const unsigned char *sdata = src->samples + sstride * (dst->y - src->y) + (dst->x - src->x) * sn;
	unsigned char *ddata = dst->samples;
	signed char map[FZ_MAX_COLORS];
	int x, y, i, j, k, n;
	unsigned char mapped[FZ_MAX_COLORS];
	int unmapped = sseps_n;
	int src_is_device_n = fz_colorspace_is_device_n(ctx, src->colorspace);
	fz_colorspace *proof_cs = (prf == src->colorspace ? NULL : prf);

	assert(da == sa);
	assert(ss == fz_count_active_separations(ctx, sseps));
	assert(ds == fz_count_active_separations(ctx, dseps));

	dstride -= dn * dw;
	sstride -= sn * dw;

	/* Process colorants (and alpha) first */
	if (dst->colorspace == src->colorspace && proof_cs == NULL)
	{
		/* Simple copy */
		unsigned char *dd = ddata;
		const unsigned char *sd = sdata;
		for (y = dh; y > 0; y--)
		{
			for (x = dw; x > 0; x--)
			{
				for (i = 0; i < dc; i++)
					dd[i] = sd[i];
				dd += dn;
				sd += sn;
				if (da)
					dd[-1] = sd[-1];
			}
			dd += dstride;
			sd += sstride;
		}
	}
	else if (src_is_device_n)
	{
		fz_color_converter cc;

		/* Init the target pixmap. */
		if (!da)
		{
			/* No alpha to worry about, just clear it. */
			fz_clear_pixmap(ctx, dst);
		}
		else if (fz_colorspace_is_subtractive(ctx, dst->colorspace))
		{
			/* Subtractive space, so copy the alpha, and set process and spot colors to 0. */
			unsigned char *dd = ddata;
			const unsigned char *sd = sdata;
			int dcs = dc + ds;
			for (y = dh; y > 0; y--)
			{
				for (x = dw; x > 0; x--)
				{
					for (i = 0; i < dcs; i++)
						dd[i] = 0;
					dd += dn;
					sd += sn;
					dd[-1] = sd[-1];
				}
				dd += dstride;
				sd += sstride;
			}
		}
		else
		{
			/* Additive space; tricky case. We need to copy the alpha, and
			 * init the process colors "full", and the spots to 0. Because
			 * we are in an additive space, and premultiplied, this means
			 * setting the process colors to alpha. */
			unsigned char *dd = ddata;
			const unsigned char *sd = sdata + sn - 1;
			int dcs = dc + ds;
			for (y = dh; y > 0; y--)
			{
				for (x = dw; x > 0; x--)
				{
					int a = *sd;
					for (i = 0; i < dc; i++)
						dd[i] = a;
					for (; i < dcs; i++)
						dd[i] = 0;
					dd[i] = a;
					dd += dn;
					sd += sn;
				}
				dd += dstride;
				sd += sstride;
			}
		}

		/* Now map the colorants down. */
		n = fz_colorspace_n(ctx, src->colorspace);

		fz_find_color_converter(ctx, &cc, src->colorspace, dst->colorspace, proof_cs, color_params);

		fz_try(ctx)
		{
			unmapped = 0;
			for (i = 0; i < n; i++)
			{
				const char *name = fz_colorspace_colorant(ctx, src->colorspace, i);

				mapped[i] = 1;

				if (name)
				{
					if (!strcmp(name, "None")) {
						mapped[i] = 0;
						continue;
					}
					if (!strcmp(name, "All"))
					{
						int n1 = dn - da;
						unsigned char *dd = ddata;
						const unsigned char *sd = sdata + i;

						for (y = dh; y > 0; y--)
						{
							for (x = dw; x > 0; x--)
							{
								unsigned char v = *sd;
								sd += sn;
								for (k = 0; k < n1; k++)
									dd[k] = v;
								dd += dn;
							}
							dd += dstride;
							sd += sstride;
						}
						continue;
					}
					for (j = 0; j < dc; j++)
					{
						const char *dname = fz_colorspace_colorant(ctx, dst->colorspace, j);
						if (dname && !strcmp(name, dname))
							goto map_device_n_spot;
					}
					for (j = 0; j < dseps_n; j++)
					{
						const char *dname = dseps->name[j];
						if (dname && !strcmp(name, dname))
						{
							j += dc;
							goto map_device_n_spot;
						}
					}
				}
				if (0)
				{
					unsigned char *dd;
					const unsigned char *sd;
	map_device_n_spot:
					/* Directly map a devicen colorant to a
					 * component (either process or spot)
					 * in the destination. */
					dd = ddata + j;
					sd = sdata + i;

					for (y = dh; y > 0; y--)
					{
						for (x = dw; x > 0; x--)
						{
							*dd = *sd;
							dd += dn;
							sd += sn;
						}
						dd += dstride;
						sd += sstride;
					}
				}
				else
				{
					unmapped = 1;
					mapped[i] = 0;
				}
			}
			if (unmapped)
			{
/* The standard spot mapping algorithm assumes that it's reasonable
 * to treat the components of deviceN spaces as being orthogonal,
 * and to add them together at the end. This avoids a color lookup
 * per pixel. The alternative mapping algorithm looks up each
 * pixel at a time, and is hence slower. */
#define ALTERNATIVE_SPOT_MAP
#ifndef ALTERNATIVE_SPOT_MAP
				for (i = 0; i < n; i++)
				{
					unsigned char *dd = ddata;
					const unsigned char *sd = sdata;
					float convert[FZ_MAX_COLORS];
					float colors[FZ_MAX_COLORS];

					if (mapped[i])
						continue;

					/* Src component i is not mapped. We need to convert that down. */
					memset(colors, 0, sizeof(float) * n);
					colors[i] = 1;
					cc.convert(ctx, &cc, colors, convert);

					if (fz_colorspace_is_subtractive(ctx, dst->colorspace))
					{
						if (sa)
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = sd[i];
									sd += sn;
									if (v != 0)
									{
										int a = dd[-1];
										for (j = 0; j < dc; j++)
											dd[j] = fz_clampi(dd[j] + v * convert[j], 0, a);
									}
									dd += dn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
						else
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = sd[i];
									if (v != 0)
									{
										for (j = 0; j < dc; j++)
											dd[j] = fz_clampi(dd[j] + v * convert[j], 0, 255);
									}
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
					}
					else
					{
						if (sa)
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = sd[i];
									sd += sn;
									if (v != 0)
									{
										int a = sd[-1];
										for (j = 0; j < dc; j++)
											dd[j] = fz_clampi(dd[j] - v * (1-convert[j]), 0, a);
									}
									dd += dn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
						else
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = sd[i];
									if (v != 0)
									{
										for (j = 0; j < dc; j++)
											dd[j] = fz_clampi(dd[j] - v * (1-convert[j]), 0, 255);
									}
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
					}
				}
#else
/* If space is subtractive then treat spots like Adobe does in Photoshop.
 * Which is to just use an equivalent CMYK value.  If we are in an additive
 * color space we will need to convert on a pixel-by-pixel basis.
 */
				float convert[FZ_MAX_COLORS];
				float colors[FZ_MAX_COLORS];

				if (fz_colorspace_is_subtractive(ctx, dst->colorspace))
				{
					for (i = 0; i < n; i++)
					{
						unsigned char *dd = ddata;
						const unsigned char *sd = sdata;

						if (mapped[i])
							continue;

						memset(colors, 0, sizeof(float) * n);
						colors[i] = 1;
						cc.convert(ctx, &cc, colors, convert);

						if (sa)
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = sd[i];
									if (v != 0)
									{
										unsigned char a = sd[sc];
										for (j = 0; j < dc; j++)
											dd[j] = fz_clampi(dd[j] + v * convert[j], 0, a);
									}
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
						else
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = sd[i];
									if (v != 0)
										for (j = 0; j < dc; j++)
											dd[j] = fz_clampi(dd[j] + v * convert[j], 0, 255);
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
					}
				}
				else
				{
					unsigned char *dd = ddata;
					const unsigned char *sd = sdata;
					if (!sa)
					{
						for (y = dh; y > 0; y--)
						{
							for (x = dw; x > 0; x--)
							{
								for (j = 0; j < n; j++)
									colors[j] = mapped[j] ? 0 : sd[j] / 255.0f;
								cc.convert(ctx, &cc, colors, convert);

								for (j = 0; j < dc; j++)
									dd[j] = fz_clampi(255 * convert[j], 0, 255);
								dd += dn;
								sd += sn;
							}
							dd += dstride;
							sd += sstride;
						}
					}
					else
					{
						for (y = dh; y > 0; y--)
						{
							for (x = dw; x > 0; x--)
							{
								unsigned char a = sd[sc];
								float inva = 1.0f/a;
								for (j = 0; j < n; j++)
									colors[j] = mapped[j] ? 0 : sd[j] * inva;
								cc.convert(ctx, &cc, colors, convert);

								for (j = 0; j < dc; j++)
									dd[j] = fz_clampi(a * convert[j], 0, a);
								dd += dn;
								sd += sn;
							}
							dd += dstride;
							sd += sstride;
						}
					}
				}
#endif
			}
		}
		fz_always(ctx)
			fz_drop_color_converter(ctx, &cc);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}
	else
	{
		/* Use a standard pixmap converter to convert the process + alpha. */
		fz_convert_pixmap_samples(ctx, src, dst, proof_cs, default_cs, fz_default_color_params, 0);

		/* And handle the spots ourselves. First make a map of what spots go where. */
		/* We want to set it up so that:
		 *    For each source spot, i, mapped[i] != 0 implies that it maps directly to a dest spot.
		 *    For each dest spot, j, mapped[j] = the source spot that goes there (or -1 if none).
		 */
		for (i = 0; i < sseps_n; i++)
			mapped[i] = 0;

		for (i = 0, k = 0; i < dseps_n; i++)
		{
			const char *name;
			int state = sep_state(dseps, i);

			if (state != FZ_SEPARATION_SPOT)
				continue;
			name = dseps->name[i];
			if (name == NULL)
				continue;
			map[k] = -1;
			for (j = 0; j < sseps_n; j++)
			{
				const char *sname;
				if (mapped[j])
					continue;
				if (sep_state(sseps, j) != FZ_SEPARATION_SPOT)
					continue;
				sname = sseps->name[j];
				if (sname && !strcmp(name, sname))
				{
					map[k] = j;
					unmapped--;
					mapped[j] = 1;
					break;
				}
			}
			k++;
		}
		if (sa)
			map[k] = sseps_n;

		/* Now we need to make d[i] = map[i] < 0 : 0 ? s[map[i]] */
		if (ds)
		{
			unsigned char *dd = ddata + dc;
			const unsigned char *sd = sdata + sc;
			for (y = dh; y > 0; y--)
			{
				for (x = dw; x > 0; x--)
				{
					for (i = 0; i < ds; i++)
						dd[i] = map[i] < 0 ? 0 : sd[map[i]];
					dd += dn;
					sd += sn;
				}
				dd += dstride;
				sd += sstride;
			}
		}

		/* So that's all the process colors, the alpha, and the
		 * directly mapped spots done. Now, are there any that
		 * remain unmapped? */
		if (unmapped)
		{
			int m;
			/* Still need to handle mapping 'lost' spots down to process colors */
			for (i = -1, m = 0; m < sseps_n; m++)
			{
				float convert[FZ_MAX_COLORS];

				if (mapped[m])
					continue;
				if (fz_separation_current_behavior(ctx, sseps, m) != FZ_SEPARATION_SPOT)
					continue;
				i++;
				/* Src spot m (the i'th one) is not mapped. We need to convert that down. */
				fz_separation_equivalent(ctx, sseps, m, dst->colorspace, convert, proof_cs, color_params);

				if (fz_colorspace_is_subtractive(ctx, dst->colorspace))
				{
					if (fz_colorspace_is_subtractive(ctx, src->colorspace))
					{
						unsigned char *dd = ddata;
						const unsigned char *sd = sdata + sc;
						if (sa)
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = sd[i];
									if (v != 0)
									{
										unsigned char a = sd[ss];
										for (k = 0; k < dc; k++)
											dd[k] = fz_clampi(dd[k] + v * convert[k], 0, a);
									}
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
						else
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = sd[i];
									if (v != 0)
										for (k = 0; k < dc; k++)
											dd[k] = fz_clampi(dd[k] + v * convert[k], 0, 255);
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
					}
					else
					{
						unsigned char *dd = ddata;
						const unsigned char *sd = sdata + sc;
						if (sa)
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = 0xff - sd[i];
									if (v != 0)
									{
										unsigned char a = sd[ss];
										for (k = 0; k < dc; k++)
											dd[k] = fz_clampi(dd[k] + v * convert[k], 0, a);
									}
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
						else
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = 0xff - sd[i];
									if (v != 0)
										for (k = 0; k < dc; k++)
											dd[k] = fz_clampi(dd[k] + v * convert[k], 0, 255);
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
					}
				}
				else
				{
					for (k = 0; k < dc; k++)
						convert[k] = 1-convert[k];
					if (fz_colorspace_is_subtractive(ctx, src->colorspace))
					{
						unsigned char *dd = ddata;
						const unsigned char *sd = sdata + sc;
						if (sa)
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = sd[i];
									if (v != 0)
									{
										unsigned char a = sd[ss];
										for (k = 0; k < dc; k++)
											dd[k] = fz_clampi(dd[k] - v * convert[k], 0, a);
									}
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
						else
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = sd[i];
									if (v != 0)
										for (k = 0; k < dc; k++)
											dd[k] = fz_clampi(dd[k] - v * convert[k], 0, 255);
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
					}
					else
					{
						unsigned char *dd = ddata;
						const unsigned char *sd = sdata + sc;
						if (sa)
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = 0xff - sd[i];
									if (v != 0)
									{
										unsigned char a = sd[ss];
										for (k = 0; k < dc; k++)
											dd[k] = fz_clampi(dd[k] - v * convert[k], 0, a);
									}
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
						else
						{
							for (y = dh; y > 0; y--)
							{
								for (x = dw; x > 0; x--)
								{
									unsigned char v = 0xff - sd[i];
									if (v != 0)
										for (k = 0; k < dc; k++)
											dd[k] = fz_clampi(dd[k] - v * convert[k], 0, 255);
									dd += dn;
									sd += sn;
								}
								dd += dstride;
								sd += sstride;
							}
						}
					}
				}
			}
		}
	}

	return dst;
}

/* Convert a color given in terms of one colorspace,
 * to a color in terms of another colorspace/separations. */
void
fz_convert_separation_colors(fz_context *ctx,
	fz_colorspace *src_cs, const float *src_color,
	fz_separations *dst_seps, fz_colorspace *dst_cs, float *dst_color,
	fz_color_params color_params)
{
	int i, j, n, dc, ds, dn, pred;
	float remainders[FZ_MAX_COLORS];
	int remaining = 0;

	assert(dst_cs && src_cs && dst_color && src_color);
	assert(fz_colorspace_is_device_n(ctx, src_cs));

	dc = fz_colorspace_n(ctx, dst_cs);
	ds = (dst_seps == NULL ? 0: dst_seps->num_separations);
	dn = dc + ds;

	i = 0;
	if (!fz_colorspace_is_subtractive(ctx, dst_cs))
		for (; i < dc; i++)
			dst_color[i] = 1;
	for (; i < dn; i++)
		dst_color[i] = 0;

	n = fz_colorspace_n(ctx, src_cs);
	pred = 0;
	for (i = 0; i < n; i++)
	{
		const char *name = fz_colorspace_colorant(ctx, src_cs, i);

		if (name == NULL)
			continue;
		if (i == 0 && !strcmp(name, "All"))
		{
			/* This is only supposed to happen in separation spaces, not DeviceN */
			if (n != 1)
				fz_warn(ctx, "All found in DeviceN space");
			for (i = 0; i < dn; i++)
				dst_color[i] = src_color[0];
			break;
		}
		if (!strcmp(name, "None"))
			continue;

		/* The most common case is that the colorant we match is the
		 * one after the one we matched before, so optimise for that. */
		for (j = pred; j < ds; j++)
		{
			const char *dname = dst_seps->name[j];
			if (dname && !strcmp(name, dname))
				goto found_sep;
		}
		for (j = 0; j < pred; j++)
		{
			const char *dname = dst_seps->name[j];
			if (dname && !strcmp(name, dname))
				goto found_sep;
		}
		for (j = 0; j < dc; j++)
		{
			const char *dname = fz_colorspace_colorant(ctx, dst_cs, j);
			if (dname && !strcmp(name, dname))
				goto found_process;
		}
		if (0) {
found_sep:
			dst_color[j+dc] = src_color[i];
			pred = j+1;
		}
		else if (0)
		{
found_process:
			dst_color[j] += src_color[i];
		}
		else
		{
			if (remaining == 0)
			{
				memset(remainders, 0, sizeof(float) * n);
				remaining = 1;
			}
			remainders[i] = src_color[i];
		}
	}

	if (remaining)
	{
		/* There were some spots that didn't copy over */
		float converted[FZ_MAX_COLORS];
		fz_convert_color(ctx, src_cs, remainders, dst_cs, converted, NULL, color_params);
		for (i = 0; i < dc; i++)
			dst_color[i] += converted[i];
	}
}

/* Get the equivalent separation color in a given colorspace. */
void
fz_separation_equivalent(fz_context *ctx,
	const fz_separations *seps,
	int i,
	fz_colorspace *dst_cs, float *convert,
	fz_colorspace *prf,
	fz_color_params color_params)
{
	float colors[FZ_MAX_COLORS];

	if (!seps->cs[i])
	{
		switch (fz_colorspace_n(ctx, dst_cs))
		{
		case 3:
			convert[0] = (seps->rgba[i] & 0xff)/ 255.0f;
			convert[1] = ((seps->rgba[i]>>8) & 0xff)/ 255.0f;
			convert[2] = ((seps->rgba[i]>>16) & 0xff)/ 255.0f;
			convert[3] = ((seps->rgba[i]>>24) & 0xff)/ 255.0f;
			return;
		case 4:
			convert[0] = (seps->cmyk[i] & 0xff)/ 255.0f;
			convert[1] = ((seps->cmyk[i]>>8) & 0xff)/ 255.0f;
			convert[2] = ((seps->cmyk[i]>>16) & 0xff)/ 255.0f;
			convert[3] = ((seps->cmyk[i]>>24) & 0xff)/ 255.0f;
			return;
		default:
			fz_throw(ctx, FZ_ERROR_GENERIC, "Cannot return equivalent in this colorspace");
		}
	}

	memset(colors, 0, sizeof(float) * fz_colorspace_n(ctx, seps->cs[i]));
	colors[seps->cs_pos[i]] = 1;
	fz_convert_color(ctx, seps->cs[i], colors, dst_cs, convert, prf, color_params);
}
