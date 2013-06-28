#include "mupdf/pdf.h"

typedef struct pdf_device_s pdf_device;

typedef struct gstate_s gstate;

struct gstate_s
{
	/* The first few entries aren't really graphics state things, but
	 * they are recorded here as they are fundamentally intertwined with
	 * the push/pulling of the gstates. */
	fz_buffer *buf;
	void (*on_pop)(pdf_device*,void *);
	void *on_pop_arg;
	/* The graphics state proper */
	fz_colorspace *colorspace[2];
	float color[2][4];
	fz_matrix ctm;
	fz_stroke_state *stroke_state;
	float alpha[2];
	int font;
	float font_size;
	float char_spacing;
	float word_spacing;
	float horizontal_scaling;
	float leading;
	int text_rendering_mode;
	float rise;
	int knockout;
	fz_matrix tm;
};

typedef struct image_entry_s image_entry;

struct image_entry_s
{
	char digest[16];
	pdf_obj *ref;
};

typedef struct alpha_entry_s alpha_entry;

struct alpha_entry_s
{
	float alpha;
	int stroke;
};

typedef struct font_entry_s font_entry;

struct font_entry_s
{
	fz_font *font;
};

typedef struct group_entry_s group_entry;

struct group_entry_s
{
	int blendmode;
	int alpha;
	int isolated;
	int knockout;
	fz_colorspace *colorspace;
	pdf_obj *ref;
};

struct pdf_device_s
{
	fz_context *ctx;
	pdf_document *doc;
	pdf_obj *contents;
	pdf_obj *resources;

	int in_text;

	int num_forms;
	int num_smasks;

	int num_gstates;
	int max_gstates;
	gstate *gstates;

	int num_imgs;
	int max_imgs;
	image_entry *images;

	int num_alphas;
	int max_alphas;
	alpha_entry *alphas;

	int num_fonts;
	int max_fonts;
	font_entry *fonts;

	int num_groups;
	int max_groups;
	group_entry *groups;
};

#define CURRENT_GSTATE(pdev) (&(pdev)->gstates[(pdev)->num_gstates-1])

/* Helper functions */

static int
send_image(pdf_device *pdev, fz_image *image, int mask, int smask)
{
	fz_context *ctx = pdev->ctx;
	fz_pixmap *pixmap = NULL;
	pdf_obj *imobj = NULL;
	pdf_obj *imref = NULL;
	fz_compressed_buffer *cbuffer = NULL;
	fz_compression_params *cp = NULL;
	fz_buffer *buffer = NULL;
	int i, num;
	fz_md5 state;
	unsigned char digest[16];
	fz_colorspace *colorspace = image->colorspace;
	pdf_document *doc = pdev->doc;

	/* If we can maintain compression, do so */
	cbuffer = image->buffer;

	fz_var(pixmap);
	fz_var(buffer);
	fz_var(imobj);
	fz_var(imref);

	fz_try(ctx)
	{
		if (cbuffer != NULL && cbuffer->params.type != FZ_IMAGE_PNG && cbuffer->params.type != FZ_IMAGE_TIFF)
		{
			buffer = fz_keep_buffer(ctx, cbuffer->buffer);
			cp = &cbuffer->params;
		}
		else
		{
			unsigned int size;
			int n;
			/* Currently, set to maintain resolution; should we consider
			 * subsampling here according to desired output res? */
			pixmap = image->get_pixmap(ctx, image, image->w, image->h);
			colorspace = pixmap->colorspace; /* May be different to image->colorspace! */
			n = (pixmap->n == 1 ? 1 : pixmap->n-1);
			size = image->w * image->h * n;
			buffer = fz_new_buffer(ctx, size);
			buffer->len = size;
			if (pixmap->n == 1)
			{
				memcpy(buffer->data, pixmap->samples, size);
			}
			else
			{
				/* Need to remove the alpha plane */
				unsigned char *d = buffer->data;
				unsigned char *s = pixmap->samples;
				int mod = n;
				while (size--)
				{
					*d++ = *s++;
					mod--;
					if (mod == 0)
						s++, mod = n;
				}
			}
		}

		fz_md5_init(&state);
		fz_md5_update(&state, buffer->data, buffer->len);
		fz_md5_final(&state, digest);
		for(i=0; i < pdev->num_imgs; i++)
		{
			if (!memcmp(&digest, pdev->images[i].digest, sizeof(16)))
			{
				num = i;
				break;
			}
		}

		if (i < pdev->num_imgs)
			break;

		if (pdev->num_imgs == pdev->max_imgs)
		{
			int newmax = pdev->max_imgs * 2;
			if (newmax == 0)
				newmax = 4;
			pdev->images = fz_resize_array(ctx, pdev->images, newmax, sizeof(*pdev->images));
			pdev->max_imgs = newmax;
		}
		num = pdev->num_imgs++;
		memcpy(pdev->images[num].digest,digest,16);
		pdev->images[num].ref = NULL; /* Will be filled in later */

		imobj = pdf_new_dict(doc, 3);
		pdf_dict_puts_drop(imobj, "Type", pdf_new_name(doc, "XObject"));
		pdf_dict_puts_drop(imobj, "Subtype", pdf_new_name(doc, "Image"));
		pdf_dict_puts_drop(imobj, "Width", pdf_new_int(doc, image->w));
		pdf_dict_puts_drop(imobj, "Height", pdf_new_int(doc, image->h));
		if (mask)
		{}
		else if (!colorspace || colorspace->n == 1)
			pdf_dict_puts_drop(imobj, "ColorSpace", pdf_new_name(doc, "DeviceGray"));
		else if (colorspace->n == 3)
			pdf_dict_puts_drop(imobj, "ColorSpace", pdf_new_name(doc, "DeviceRGB"));
		else if (colorspace->n == 4)
			pdf_dict_puts_drop(imobj, "ColorSpace", pdf_new_name(doc, "DeviceCMYK"));
		if (!mask)
			pdf_dict_puts_drop(imobj, "BitsPerComponent", pdf_new_int(doc, image->bpc));
		switch (cp ? cp->type : FZ_IMAGE_UNKNOWN)
		{
		case FZ_IMAGE_UNKNOWN: /* Unknown also means raw */
		default:
			break;
		case FZ_IMAGE_JPEG:
			if (cp->u.jpeg.color_transform != -1)
				pdf_dict_puts_drop(imobj, "ColorTransform", pdf_new_int(doc, cp->u.jpeg.color_transform));
			pdf_dict_puts_drop(imobj, "Filter", pdf_new_name(doc, "DCTDecode"));
			break;
		case FZ_IMAGE_JPX:
			if (cp->u.jpx.smask_in_data)
				pdf_dict_puts_drop(imobj, "SMaskInData", pdf_new_int(doc, cp->u.jpx.smask_in_data));
			pdf_dict_puts_drop(imobj, "Filter", pdf_new_name(doc, "JPXDecode"));
			break;
		case FZ_IMAGE_FAX:
			if (cp->u.fax.columns)
				pdf_dict_puts(imobj, "Columns", pdf_new_int(doc, cp->u.fax.columns));
			if (cp->u.fax.rows)
				pdf_dict_puts(imobj, "Rows", pdf_new_int(doc, cp->u.fax.rows));
			if (cp->u.fax.k)
				pdf_dict_puts(imobj, "K", pdf_new_int(doc, cp->u.fax.k));
			if (cp->u.fax.end_of_line)
				pdf_dict_puts(imobj, "EndOfLine", pdf_new_int(doc, cp->u.fax.end_of_line));
			if (cp->u.fax.encoded_byte_align)
				pdf_dict_puts(imobj, "EncodedByteAlign", pdf_new_int(doc, cp->u.fax.encoded_byte_align));
			if (cp->u.fax.end_of_block)
				pdf_dict_puts(imobj, "EndOfBlock", pdf_new_int(doc, cp->u.fax.end_of_block));
			if (cp->u.fax.black_is_1)
				pdf_dict_puts(imobj, "BlackIs1", pdf_new_int(doc, cp->u.fax.black_is_1));
			if (cp->u.fax.damaged_rows_before_error)
				pdf_dict_puts(imobj, "DamagedRowsBeforeError", pdf_new_int(doc, cp->u.fax.damaged_rows_before_error));
			pdf_dict_puts(imobj, "Filter", pdf_new_name(doc, "CCITTFaxDecode"));
			break;
		case FZ_IMAGE_JBIG2:
			/* FIXME - jbig2globals */
			cp->type = FZ_IMAGE_UNKNOWN;
			break;
		case FZ_IMAGE_FLATE:
			if (cp->u.flate.columns)
				pdf_dict_puts(imobj, "Columns", pdf_new_int(doc, cp->u.flate.columns));
			if (cp->u.flate.colors)
				pdf_dict_puts(imobj, "Colors", pdf_new_int(doc, cp->u.flate.colors));
			if (cp->u.flate.predictor)
				pdf_dict_puts(imobj, "Predictor", pdf_new_int(doc, cp->u.flate.predictor));
			pdf_dict_puts(imobj, "Filter", pdf_new_name(doc, "FlateDecode"));
			pdf_dict_puts_drop(imobj, "BitsPerComponent", pdf_new_int(doc, image->bpc));
			break;
		case FZ_IMAGE_LZW:
			if (cp->u.lzw.columns)
				pdf_dict_puts(imobj, "Columns", pdf_new_int(doc, cp->u.lzw.columns));
			if (cp->u.lzw.colors)
				pdf_dict_puts(imobj, "Colors", pdf_new_int(doc, cp->u.lzw.colors));
			if (cp->u.lzw.predictor)
				pdf_dict_puts(imobj, "Predictor", pdf_new_int(doc, cp->u.lzw.predictor));
			if (cp->u.lzw.early_change)
				pdf_dict_puts(imobj, "EarlyChange", pdf_new_int(doc, cp->u.lzw.early_change));
			pdf_dict_puts(imobj, "Filter", pdf_new_name(doc, "LZWDecode"));
			break;
		case FZ_IMAGE_RLD:
			pdf_dict_puts(imobj, "Filter", pdf_new_name(doc, "RunLengthDecode"));
			break;
		}
		if (mask)
		{
			pdf_dict_puts_drop(imobj, "ImageMask", pdf_new_bool(doc, 1));
		}
		if (image->mask)
		{
			int smasknum = send_image(pdev, image->mask, 0, 1);
			pdf_dict_puts(imobj, "SMask", pdev->images[smasknum].ref);
		}

		imref = pdf_new_ref(doc, imobj);
		pdf_update_stream(doc, pdf_to_num(imref), buffer);
		pdf_dict_puts_drop(imobj, "Length", pdf_new_int(doc, buffer->len));

		{
			char text[32];
			snprintf(text, sizeof(text), "XObject/Img%d", num);
			pdf_dict_putp(pdev->resources, text, imref);
		}
		pdev->images[num].ref = imref;
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buffer);
		pdf_drop_obj(imobj);
		fz_drop_pixmap(ctx, pixmap);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(imref);
		fz_rethrow(ctx);
	}
	return num;
}

static void
pdf_dev_stroke_state(pdf_device *pdev, fz_stroke_state *stroke_state)
{
	fz_context *ctx = pdev->ctx;
	gstate *gs = CURRENT_GSTATE(pdev);

	if (stroke_state == gs->stroke_state)
		return;
	if (gs->stroke_state && !memcmp(stroke_state, gs->stroke_state, sizeof(*stroke_state)))
		return;
	if (!gs->stroke_state || gs->stroke_state->linewidth != stroke_state->linewidth)
	{
		fz_buffer_printf(ctx, gs->buf, "%f w\n", stroke_state->linewidth);
	}
	if (!gs->stroke_state || gs->stroke_state->start_cap != stroke_state->start_cap)
	{
		int cap = stroke_state->start_cap;
		/* FIXME: Triangle caps aren't supported in pdf */
		if (cap == FZ_LINECAP_TRIANGLE)
			cap = FZ_LINECAP_BUTT;
		fz_buffer_printf(ctx, gs->buf, "%d J\n", cap);
	}
	if (!gs->stroke_state || gs->stroke_state->linejoin != stroke_state->linejoin)
	{
		int join = stroke_state->linejoin;
		if (join == FZ_LINEJOIN_MITER_XPS)
			join = FZ_LINEJOIN_MITER;
		fz_buffer_printf(ctx, gs->buf, "%d j\n", join);
	}
	if (!gs->stroke_state || gs->stroke_state->miterlimit != stroke_state->miterlimit)
	{
		fz_buffer_printf(ctx, gs->buf, "%f M\n", stroke_state->miterlimit);
	}
	if (gs->stroke_state == NULL && stroke_state->dash_len == 0)
	{}
	else if (!gs->stroke_state || gs->stroke_state->dash_phase != stroke_state->dash_phase || gs->stroke_state->dash_len != stroke_state->dash_len ||
		memcmp(gs->stroke_state->dash_list, stroke_state->dash_list, sizeof(float)*stroke_state->dash_len))
	{
		int i;
		if (stroke_state->dash_len == 0)
			fz_buffer_printf(ctx, gs->buf, "[");
		for (i = 0; i < stroke_state->dash_len; i++)
			fz_buffer_printf(ctx, gs->buf, "%c%f", (i == 0 ? '[' : ' '), stroke_state->dash_list[i]);
		fz_buffer_printf(ctx, gs->buf, "]%f d\n", stroke_state->dash_phase);

	}
	fz_drop_stroke_state(ctx, gs->stroke_state);
	gs->stroke_state = fz_keep_stroke_state(ctx, stroke_state);
}

static void
pdf_dev_path(pdf_device *pdev, fz_path *path)
{
	fz_context *ctx = pdev->ctx;
	gstate *gs = CURRENT_GSTATE(pdev);
	float x, y;
	int i = 0;
	while (i < path->len)
	{
		switch (path->items[i++].k)
		{
		case FZ_MOVETO:
			x = path->items[i++].v;
			y = path->items[i++].v;
			fz_buffer_printf(ctx, gs->buf, "%f %f m\n", x, y);
			break;
		case FZ_LINETO:
			x = path->items[i++].v;
			y = path->items[i++].v;
			fz_buffer_printf(ctx, gs->buf, "%f %f l\n", x, y);
			break;
		case FZ_CURVETO:
			x = path->items[i++].v;
			y = path->items[i++].v;
			fz_buffer_printf(ctx, gs->buf, "%f %f ", x, y);
			x = path->items[i++].v;
			y = path->items[i++].v;
			fz_buffer_printf(ctx, gs->buf, "%f %f ", x, y);
			x = path->items[i++].v;
			y = path->items[i++].v;
			fz_buffer_printf(ctx, gs->buf, "%f %f c\n", x, y);
			break;
		case FZ_CLOSE_PATH:
			fz_buffer_printf(ctx, gs->buf, "h\n");
			break;
		}
	}
}

static void
pdf_dev_ctm(pdf_device *pdev, const fz_matrix *ctm)
{
	fz_matrix inverse;
	gstate *gs = CURRENT_GSTATE(pdev);

	if (memcmp(&gs->ctm, ctm, sizeof(*ctm)) == 0)
		return;
	fz_invert_matrix(&inverse, &gs->ctm);
	fz_concat(&inverse, ctm, &inverse);
	memcpy(&gs->ctm, ctm, sizeof(*ctm));
	fz_buffer_printf(pdev->ctx, gs->buf, "%f %f %f %f %f %f cm\n", inverse.a, inverse.b, inverse.c, inverse.d, inverse.e, inverse.f);
}

static void
pdf_dev_color(pdf_device *pdev, fz_colorspace *colorspace, float *color, int stroke)
{
	int diff = 0;
	int i;
	int cspace = 0;
	fz_context *ctx = pdev->ctx;
	float rgb[FZ_MAX_COLORS];
	gstate *gs = CURRENT_GSTATE(pdev);

	if (colorspace == fz_device_gray(ctx))
		cspace = 1;
	else if (colorspace == fz_device_rgb(ctx))
		cspace = 3;
	else if (colorspace == fz_device_cmyk(ctx))
		cspace = 4;

	if (cspace == 0)
	{
		/* If it's an unknown colorspace, fallback to rgb */
		colorspace->to_rgb(ctx, colorspace, color, rgb);
		color = rgb;
		colorspace = fz_device_rgb(ctx);
	}

	if (gs->colorspace[stroke] != colorspace)
	{
		gs->colorspace[stroke] = colorspace;
		diff = 1;
	}

	for (i=0; i < colorspace->n; i++)
		if (gs->color[stroke][i] != color[i])
		{
			gs->color[stroke][i] = color[i];
			diff = 1;
		}

	if (diff == 0)
		return;

	switch (cspace + stroke*8)
	{
		case 1:
			fz_buffer_printf(ctx, gs->buf, "%f g\n", color[0]);
			break;
		case 3:
			fz_buffer_printf(ctx, gs->buf, "%f %f %f rg\n", color[0], color[1], color[2]);
			break;
		case 4:
			fz_buffer_printf(ctx, gs->buf, "%f %f %f %f k\n", color[0], color[1], color[2], color[3]);
			break;
		case 1+8:
			fz_buffer_printf(ctx, gs->buf, "%f G\n", color[0]);
			break;
		case 3+8:
			fz_buffer_printf(ctx, gs->buf, "%f %f %f RG\n", color[0], color[1], color[2]);
			break;
		case 4+8:
			fz_buffer_printf(ctx, gs->buf, "%f %f %f %f K\n", color[0], color[1], color[2], color[3]);
			break;
	}
}

static void
pdf_dev_alpha(pdf_device *pdev, float alpha, int stroke)
{
	int i;
	fz_context *ctx = pdev->ctx;
	pdf_document *doc = pdev->doc;
	gstate *gs = CURRENT_GSTATE(pdev);

	/* If the alpha is unchanged, nothing to do */
	if (gs->alpha[stroke] == alpha)
		return;

	/* Have we sent such an alpha before? */
	for (i = 0; i < pdev->num_alphas; i++)
		if (pdev->alphas[i].alpha == alpha && pdev->alphas[i].stroke == stroke)
			break;

	if (i == pdev->num_alphas)
	{
		pdf_obj *o;
		pdf_obj *ref = NULL;

		fz_var(ref);

		/* No. Need to make a new one */
		if (pdev->num_alphas == pdev->max_alphas)
		{
			int newmax = pdev->max_alphas * 2;
			if (newmax == 0)
				newmax = 4;
			pdev->alphas = fz_resize_array(ctx, pdev->alphas, newmax, sizeof(*pdev->alphas));
			pdev->max_alphas = newmax;
		}
		pdev->alphas[i].alpha = alpha;
		pdev->alphas[i].stroke = stroke;

		o = pdf_new_dict(doc, 1);
		fz_try(ctx)
		{
			char text[32];
			pdf_dict_puts_drop(o, (stroke ? "CA" : "ca"), pdf_new_real(doc, alpha));
			ref = pdf_new_ref(doc, o);
			snprintf(text, sizeof(text), "ExtGState/Alp%d", i);
			pdf_dict_putp(pdev->resources, text, ref);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(o);
			pdf_drop_obj(ref);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
		pdev->num_alphas++;
	}
	fz_buffer_printf(ctx, gs->buf, "/Alp%d gs\n", i);
}

static void
pdf_dev_font(pdf_device *pdev, fz_font *font, float size)
{
	int i;
	pdf_document *doc = pdev->doc;
	fz_context *ctx = pdev->ctx;
	gstate *gs = CURRENT_GSTATE(pdev);

	/* If the font is unchanged, nothing to do */
	if (gs->font >= 0 && pdev->fonts[gs->font].font == font)
		return;

	/* Have we sent such a font before? */
	for (i = 0; i < pdev->num_fonts; i++)
		if (pdev->fonts[i].font == font)
			break;

	if (i == pdev->num_fonts)
	{
		pdf_obj *o;
		pdf_obj *ref = NULL;

		fz_var(ref);

		/* No. Need to make a new one */
		if (pdev->num_fonts == pdev->max_fonts)
		{
			int newmax = pdev->max_fonts * 2;
			if (newmax == 0)
				newmax = 4;
			pdev->fonts = fz_resize_array(ctx, pdev->fonts, newmax, sizeof(*pdev->fonts));
			pdev->max_fonts = newmax;
		}
		pdev->fonts[i].font = fz_keep_font(ctx, font);

		o = pdf_new_dict(doc, 3);
		fz_try(ctx)
		{
			/* BIG FIXME: Get someone who understands fonts to fill this bit in. */
			char text[32];
			pdf_dict_puts_drop(o, "Type", pdf_new_name(doc, "Font"));
			pdf_dict_puts_drop(o, "Subtype", pdf_new_name(doc, "Type1"));
			pdf_dict_puts_drop(o, "BaseFont", pdf_new_name(doc, "Helvetica"));
			ref = pdf_new_ref(doc, o);
			snprintf(text, sizeof(text), "Font/F%d", i);
			pdf_dict_putp(pdev->resources, text, ref);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(o);
			pdf_drop_obj(ref);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
		pdev->num_fonts++;
	}
	fz_buffer_printf(ctx, gs->buf, "/F%d %f Tf\n", i, size);
}

static void
pdf_dev_tm(pdf_device *pdev, const fz_matrix *tm)
{
	gstate *gs = CURRENT_GSTATE(pdev);

	if (memcmp(&gs->tm, tm, sizeof(*tm)) == 0)
		return;
	fz_buffer_printf(pdev->ctx, gs->buf, "%f %f %f %f %f %f Tm\n", tm->a, tm->b, tm->c, tm->d, tm->e, tm->f);
	gs->tm = *tm;
}

static void
pdf_dev_push_new_buf(pdf_device *pdev, fz_buffer *buf, void (*on_pop)(pdf_device*,void *), void *on_pop_arg)
{
	fz_context *ctx = pdev->ctx;

	if (pdev->num_gstates == pdev->max_gstates)
	{
		int newmax = pdev->max_gstates*2;

		pdev->gstates = fz_resize_array(ctx, pdev->gstates, newmax, sizeof(*pdev->gstates));
		pdev->max_gstates = newmax;
	}
	memcpy(&pdev->gstates[pdev->num_gstates], &pdev->gstates[pdev->num_gstates-1], sizeof(*pdev->gstates));
	fz_keep_stroke_state(ctx, pdev->gstates[pdev->num_gstates].stroke_state);
	if (buf)
		pdev->gstates[pdev->num_gstates].buf = buf;
	else
		fz_keep_buffer(ctx, pdev->gstates[pdev->num_gstates].buf);
	pdev->gstates[pdev->num_gstates].on_pop = on_pop;
	pdev->gstates[pdev->num_gstates].on_pop_arg = on_pop_arg;
	fz_buffer_printf(ctx, pdev->gstates[pdev->num_gstates].buf, "q\n");
	pdev->num_gstates++;
}

static void
pdf_dev_push(pdf_device *pdev)
{
	pdf_dev_push_new_buf(pdev, NULL, NULL, NULL);
}

static void *
pdf_dev_pop(pdf_device *pdev)
{
	fz_context *ctx = pdev->ctx;
	gstate *gs = CURRENT_GSTATE(pdev);
	void *arg = gs->on_pop_arg;

	fz_buffer_printf(pdev->ctx, gs->buf, "Q\n");
	if (gs->on_pop)
		gs->on_pop(pdev, arg);
	pdev->num_gstates--;
	fz_drop_stroke_state(ctx, pdev->gstates[pdev->num_gstates].stroke_state);
	fz_drop_buffer(ctx, pdev->gstates[pdev->num_gstates].buf);
	return arg;
}

static void
pdf_dev_text(pdf_device *pdev, fz_text *text)
{
	int i;
	fz_matrix trm;
	fz_matrix inverse;
	gstate *gs = CURRENT_GSTATE(pdev);
	fz_matrix trunc_trm;

	/* BIG FIXME: Get someone who understands fonts to fill this bit in. */
	trm = gs->tm;
	trunc_trm.a = trm.a;
	trunc_trm.b = trm.b;
	trunc_trm.c = trm.c;
	trunc_trm.d = trm.d;
	trunc_trm.e = 0;
	trunc_trm.f = 0;
	fz_invert_matrix(&inverse, &trunc_trm);

	for (i=0; i < text->len; i++)
	{
		fz_text_item *it = &text->items[i];
		fz_point delta;
		delta.x = it->x - trm.e;
		delta.y = it->y - trm.f;
		fz_transform_point(&delta, &inverse);
		if (delta.x != 0 || delta.y != 0)
		{
			fz_buffer_printf(pdev->ctx, gs->buf, "%f %f Td ", delta.x, delta.y);
			trm.e = it->x;
			trm.f = it->y;
		}
		fz_buffer_printf(pdev->ctx, gs->buf, "<%02x> Tj\n", it->ucs);
		/* FIXME: Advance the text position - doesn't matter at the
		 * moment as we absolutely position each glyph, but we should
		 * use more efficient text outputting where possible. */
	}
	gs->tm.e = trm.e;
	gs->tm.f = trm.f;
}

static void
pdf_dev_trm(pdf_device *pdev, int trm)
{
	gstate *gs = CURRENT_GSTATE(pdev);

	if (gs->text_rendering_mode == trm)
		return;
	gs->text_rendering_mode = trm;
	fz_buffer_printf(pdev->ctx, gs->buf, "%d Tr\n", trm);
}

static void
pdf_dev_begin_text(pdf_device *pdev, const fz_matrix *tm, int trm)
{
	pdf_dev_trm(pdev, trm);
	if (!pdev->in_text)
	{
		gstate *gs = CURRENT_GSTATE(pdev);
		fz_buffer_printf(pdev->ctx, gs->buf, "BT\n");
		gs->tm.a = 1;
		gs->tm.b = 0;
		gs->tm.c = 0;
		gs->tm.d = 1;
		gs->tm.e = 0;
		gs->tm.f = 0;
		pdev->in_text = 1;
	}
	pdf_dev_tm(pdev, tm);
}

static void
pdf_dev_end_text(pdf_device *pdev)
{
	gstate *gs = CURRENT_GSTATE(pdev);

	if (!pdev->in_text)
		return;
	pdev->in_text = 0;
	fz_buffer_printf(pdev->ctx, gs->buf, "ET\n");
}

static int
pdf_dev_new_form(pdf_obj **form_ref, pdf_device *pdev, const fz_rect *bbox, int isolated, int knockout, int blendmode, float alpha, fz_colorspace *colorspace)
{
	fz_context *ctx = pdev->ctx;
	pdf_document *doc = pdev->doc;
	int num;
	pdf_obj *group_ref;
	pdf_obj *group;
	pdf_obj *form;

	*form_ref = NULL;

	/* Find (or make) a new group with the required options. */
	for(num = 0; num < pdev->num_groups; num++)
	{
		group_entry *g = &pdev->groups[num];
		if (g->isolated == isolated && g->knockout == knockout && g->blendmode == blendmode && g->alpha == alpha && g->colorspace == colorspace)
		{
			group_ref = pdev->groups[num].ref;
			break;
		}
	}

	/* If we didn't find one, make one */
	if (num == pdev->num_groups)
	{
		if (pdev->num_groups == pdev->max_groups)
		{
			int newmax = pdev->max_groups * 2;
			if (newmax == 0)
				newmax = 4;
			pdev->groups = fz_resize_array(ctx, pdev->groups, newmax, sizeof(*pdev->groups));
			pdev->max_groups = newmax;
		}
		pdev->num_groups++;
		pdev->groups[num].isolated = isolated;
		pdev->groups[num].knockout = knockout;
		pdev->groups[num].blendmode = blendmode;
		pdev->groups[num].alpha = alpha;
		pdev->groups[num].colorspace = fz_keep_colorspace(ctx, colorspace);
		pdev->groups[num].ref = NULL;
		group = pdf_new_dict(doc, 5);
		fz_try(ctx)
		{
			pdf_dict_puts_drop(group, "Type", pdf_new_name(doc, "Group"));
			pdf_dict_puts_drop(group, "S", pdf_new_name(doc, "Transparency"));
			pdf_dict_puts_drop(group, "K", pdf_new_bool(doc, knockout));
			pdf_dict_puts_drop(group, "I", pdf_new_bool(doc, isolated));
			pdf_dict_puts_drop(group, "K", pdf_new_bool(doc, knockout));
			pdf_dict_puts_drop(group, "BM", pdf_new_name(doc, fz_blendmode_name(blendmode)));
			if (!colorspace)
			{}
			else if (colorspace->n == 1)
				pdf_dict_puts_drop(group, "CS", pdf_new_name(doc, "DeviceGray"));
			else if (colorspace->n == 4)
				pdf_dict_puts_drop(group, "CS", pdf_new_name(doc, "DeviceCMYK"));
			else
				pdf_dict_puts_drop(group, "CS", pdf_new_name(doc, "DeviceRGB"));
			group_ref = pdev->groups[num].ref = pdf_new_ref(doc, group);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(group);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
	}

	/* Make us a new Forms object that points to that group, and change
	 * to writing into the buffer for that Forms object. */
	form = pdf_new_dict(doc, 4);
	fz_try(ctx)
	{
		pdf_dict_puts_drop(form, "Subtype", pdf_new_name(doc, "Form"));
		pdf_dict_puts(form, "Group", group_ref);
		pdf_dict_puts_drop(form, "FormType", pdf_new_int(doc, 1));
		pdf_dict_puts_drop(form, "BBox", pdf_new_rect(doc, bbox));
		*form_ref = pdf_new_ref(doc, form);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(form);
		fz_rethrow(ctx);
	}

	/* Insert the new form object into the resources */
	{
		char text[32];
		num = pdev->num_forms++;
		snprintf(text, sizeof(text), "XObject/Fm%d", num);
		pdf_dict_putp(pdev->resources, text, *form_ref);
	}

	return num;
}

/* Entry points */

static void
pdf_dev_fill_path(fz_device *dev, fz_path *path, int even_odd, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	pdf_device *pdev = dev->user;
	gstate *gs = CURRENT_GSTATE(pdev);

	pdf_dev_end_text(pdev);
	pdf_dev_alpha(pdev, alpha, 0);
	pdf_dev_color(pdev, colorspace, color, 0);
	pdf_dev_ctm(pdev, ctm);
	pdf_dev_path(pdev, path);
	fz_buffer_printf(dev->ctx, gs->buf, (even_odd ? "f*\n" : "f\n"));
}

static void
pdf_dev_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	pdf_device *pdev = dev->user;
	gstate *gs = CURRENT_GSTATE(pdev);

	pdf_dev_end_text(pdev);
	pdf_dev_alpha(pdev, alpha, 1);
	pdf_dev_color(pdev, colorspace, color, 1);
	pdf_dev_ctm(pdev, ctm);
	pdf_dev_stroke_state(pdev, stroke);
	pdf_dev_path(pdev, path);
	fz_buffer_printf(dev->ctx, gs->buf, "S\n");
}

static void
pdf_dev_clip_path(fz_device *dev, fz_path *path, const fz_rect *rect, int even_odd, const fz_matrix *ctm)
{
	pdf_device *pdev = dev->user;
	gstate *gs;

	pdf_dev_end_text(pdev);
	pdf_dev_push(pdev);
	pdf_dev_ctm(pdev, ctm);
	pdf_dev_path(pdev, path);
	gs = CURRENT_GSTATE(pdev);
	fz_buffer_printf(dev->ctx, gs->buf, (even_odd ? "W* n\n" : "W n\n"));
}

static void
pdf_dev_clip_stroke_path(fz_device *dev, fz_path *path, const fz_rect *rect, fz_stroke_state *stroke, const fz_matrix *ctm)
{
	pdf_device *pdev = dev->user;
	gstate *gs;

	pdf_dev_end_text(pdev);
	pdf_dev_push(pdev);
	/* FIXME: Need to push a group, select a pattern (or shading) here,
	 * stroke with the pattern/shading. Then move to defining that pattern
	 * with the next calls to the device interface until the next pop
	 * when we pop the group. */
	pdf_dev_ctm(pdev, ctm);
	pdf_dev_path(pdev, path);
	gs = CURRENT_GSTATE(pdev);
	fz_buffer_printf(dev->ctx, gs->buf, "W n\n");
}

static void
pdf_dev_fill_text(fz_device *dev, fz_text *text, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	pdf_device *pdev = dev->user;

	pdf_dev_begin_text(pdev, &text->trm, 0);
	pdf_dev_font(pdev, text->font, 1);
	pdf_dev_ctm(pdev, ctm);
	pdf_dev_alpha(pdev, alpha, 0);
	pdf_dev_color(pdev, colorspace, color, 0);
	pdf_dev_text(pdev, text);
}

static void
pdf_dev_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm,
	fz_colorspace *colorspace, float *color, float alpha)
{
	pdf_device *pdev = dev->user;

	pdf_dev_begin_text(pdev, &text->trm, 1);
	pdf_dev_font(pdev, text->font, 1);
	pdf_dev_ctm(pdev, ctm);
	pdf_dev_alpha(pdev, alpha, 1);
	pdf_dev_color(pdev, colorspace, color, 1);
	pdf_dev_text(pdev, text);
}

static void
pdf_dev_clip_text(fz_device *dev, fz_text *text, const fz_matrix *ctm, int accumulate)
{
	pdf_device *pdev = dev->user;

	pdf_dev_begin_text(pdev, &text->trm, 0);
	pdf_dev_ctm(pdev, ctm);
	pdf_dev_font(pdev, text->font, 7);
	pdf_dev_text(pdev, text);
}

static void
pdf_dev_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, const fz_matrix *ctm)
{
	pdf_device *pdev = dev->user;

	pdf_dev_begin_text(pdev, &text->trm, 0);
	pdf_dev_font(pdev, text->font, 5);
	pdf_dev_ctm(pdev, ctm);
	pdf_dev_text(pdev, text);
}

static void
pdf_dev_ignore_text(fz_device *dev, fz_text *text, const fz_matrix *ctm)
{
	pdf_device *pdev = dev->user;

	pdf_dev_begin_text(pdev, &text->trm, 0);
	pdf_dev_ctm(pdev, ctm);
	pdf_dev_font(pdev, text->font, 3);
	pdf_dev_text(pdev, text);
}

static void
pdf_dev_fill_image(fz_device *dev, fz_image *image, const fz_matrix *ctm, float alpha)
{
	pdf_device *pdev = (pdf_device *)dev->user;
	int num;
	gstate *gs = CURRENT_GSTATE(pdev);
	fz_matrix local_ctm = *ctm;

	pdf_dev_end_text(pdev);
	num = send_image(pdev, image, 0, 0);
	pdf_dev_alpha(pdev, alpha, 0);
	/* PDF images are upside down, so fiddle the ctm */
	fz_pre_scale(&local_ctm, 1, -1);
	fz_pre_translate(&local_ctm, 0, -1);
	pdf_dev_ctm(pdev, &local_ctm);
	fz_buffer_printf(dev->ctx, gs->buf, "/Img%d Do\n", num);
}

static void
pdf_dev_fill_shade(fz_device *dev, fz_shade *shade, const fz_matrix *ctm, float alpha)
{
	pdf_device *pdev = (pdf_device *)dev->user;

	/* FIXME */
	pdf_dev_end_text(pdev);
}

static void
pdf_dev_fill_image_mask(fz_device *dev, fz_image *image, const fz_matrix *ctm,
fz_colorspace *colorspace, float *color, float alpha)
{
	pdf_device *pdev = (pdf_device *)dev->user;
	gstate *gs = CURRENT_GSTATE(pdev);
	int num;
	fz_matrix local_ctm = *ctm;

	pdf_dev_end_text(pdev);
	num = send_image(pdev, image, 1, 0);
	fz_buffer_printf(dev->ctx, gs->buf, "q\n");
	pdf_dev_alpha(pdev, alpha, 0);
	pdf_dev_color(pdev, colorspace, color, 0);
	/* PDF images are upside down, so fiddle the ctm */
	fz_pre_scale(&local_ctm, 1, -1);
	fz_pre_translate(&local_ctm, 0, -1);
	pdf_dev_ctm(pdev, &local_ctm);
	fz_buffer_printf(dev->ctx, gs->buf, "/Img%d Do Q\n", num);
}

static void
pdf_dev_clip_image_mask(fz_device *dev, fz_image *image, const fz_rect *rect, const fz_matrix *ctm)
{
	pdf_device *pdev = (pdf_device *)dev->user;

	/* FIXME */
	pdf_dev_end_text(pdev);
	pdf_dev_push(pdev);
}

static void
pdf_dev_pop_clip(fz_device *dev)
{
	pdf_device *pdev = (pdf_device *)dev->user;

	/* FIXME */
	pdf_dev_end_text(pdev);
	pdf_dev_pop(pdev);
}

static void
pdf_dev_begin_mask(fz_device *dev, const fz_rect *bbox, int luminosity, fz_colorspace *colorspace, float *color)
{
	pdf_device *pdev = (pdf_device *)dev->user;
	pdf_document *doc = pdev->doc;
	fz_context *ctx = pdev->ctx;
	gstate *gs;
	pdf_obj *smask = NULL;
	pdf_obj *egs = NULL;
	pdf_obj *egs_ref;
	pdf_obj *form_ref;
	pdf_obj *color_obj = NULL;
	int i;

	fz_var(smask);
	fz_var(egs);
	fz_var(color_obj);

	pdf_dev_end_text(pdev);

	/* Make a new form to contain the contents of the softmask */
	pdf_dev_new_form(&form_ref, pdev, bbox, 0, 0, 0, 1, colorspace);

	fz_try(ctx)
	{
		smask = pdf_new_dict(doc, 4);
		pdf_dict_puts(smask, "Type", pdf_new_name(doc, "Mask"));
		pdf_dict_puts_drop(smask, "S", pdf_new_name(doc, (luminosity ? "Luminosity" : "Alpha")));
		pdf_dict_puts(smask, "G", form_ref);
		color_obj = pdf_new_array(doc, colorspace->n);
		for (i = 0; i < colorspace->n; i++)
			pdf_array_push(color_obj, pdf_new_real(doc, color[i]));
		pdf_dict_puts_drop(smask, "BC", color_obj);
		color_obj = NULL;

		egs = pdf_new_dict(doc, 5);
		pdf_dict_puts_drop(egs, "Type", pdf_new_name(doc, "ExtGState"));
		pdf_dict_puts_drop(egs, "SMask", pdf_new_ref(doc, smask));
		egs_ref = pdf_new_ref(doc, egs);

		{
			char text[32];
			snprintf(text, sizeof(text), "ExtGState/SM%d", pdev->num_smasks++);
			pdf_dict_putp(pdev->resources, text, egs_ref);
			pdf_drop_obj(egs_ref);
		}
		gs = CURRENT_GSTATE(pdev);
		fz_buffer_printf(dev->ctx, gs->buf, "/SM%d gs\n", pdev->num_smasks-1);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(smask);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(form_ref);
		pdf_drop_obj(color_obj);
		fz_rethrow(ctx);
	}

	/* Now, everything we get until the end_mask needs to go into a
	 * new buffer, which will be the stream contents for the form. */
	pdf_dev_push_new_buf(pdev, fz_new_buffer(ctx, 1024), NULL, form_ref);
}

static void
pdf_dev_end_mask(fz_device *dev)
{
	pdf_device *pdev = (pdf_device *)dev->user;
	pdf_document *doc = pdev->doc;
	fz_context *ctx = pdev->ctx;
	gstate *gs = CURRENT_GSTATE(pdev);
	fz_buffer *buf = fz_keep_buffer(ctx, gs->buf);
	pdf_obj *form_ref = (pdf_obj *)gs->on_pop_arg;

	/* Here we do part of the pop, but not all of it. */
	pdf_dev_end_text(pdev);
	fz_buffer_printf(ctx, buf, "Q\n");
	pdf_dict_puts_drop(form_ref, "Length", pdf_new_int(doc, buf->len));
	pdf_update_stream(doc, pdf_to_num(form_ref), buf);
	fz_drop_buffer(ctx, buf);
	gs->buf = fz_keep_buffer(ctx, gs[-1].buf);
	gs->on_pop_arg = NULL;
	pdf_drop_obj(form_ref);
	fz_buffer_printf(ctx, gs->buf, "q\n");
}

static void
pdf_dev_begin_group(fz_device *dev, const fz_rect *bbox, int isolated, int knockout, int blendmode, float alpha)
{
	pdf_device *pdev = (pdf_device *)dev->user;
	fz_context *ctx = pdev->ctx;
	int num;
	pdf_obj *form_ref;
	gstate *gs;

	pdf_dev_end_text(pdev);

	num = pdf_dev_new_form(&form_ref, pdev, bbox, isolated, knockout, blendmode, alpha, NULL);

	/* Add the call to this group */
	gs = CURRENT_GSTATE(pdev);
	fz_buffer_printf(dev->ctx, gs->buf, "/Fm%d Do\n", num);

	/* Now, everything we get until the end of group needs to go into a
	 * new buffer, which will be the stream contents for the form. */
	pdf_dev_push_new_buf(pdev, fz_new_buffer(ctx, 1024), NULL, form_ref);
}

static void
pdf_dev_end_group(fz_device *dev)
{
	pdf_device *pdev = (pdf_device *)dev->user;
	pdf_document *doc = pdev->doc;
	gstate *gs = CURRENT_GSTATE(pdev);
	fz_context *ctx = pdev->ctx;
	fz_buffer *buf = fz_keep_buffer(ctx, gs->buf);
	pdf_obj *form_ref;

	pdf_dev_end_text(pdev);
	form_ref = (pdf_obj *)pdf_dev_pop(pdev);
	pdf_dict_puts_drop(form_ref, "Length", pdf_new_int(doc, gs->buf->len));
	pdf_update_stream(doc, pdf_to_num(form_ref), buf);
	fz_drop_buffer(ctx, buf);
	pdf_drop_obj(form_ref);
}

static int
pdf_dev_begin_tile(fz_device *dev, const fz_rect *area, const fz_rect *view, float xstep, float ystep, const fz_matrix *ctm, int id)
{
	pdf_device *pdev = (pdf_device *)dev->user;

	/* FIXME */
	pdf_dev_end_text(pdev);
	return 0;
}

static void
pdf_dev_end_tile(fz_device *dev)
{
	pdf_device *pdev = (pdf_device *)dev->user;

	/* FIXME */
	pdf_dev_end_text(pdev);
}

static void
pdf_dev_free_user(fz_device *dev)
{
	pdf_device *pdev = dev->user;
	pdf_document *doc = pdev->doc;
	fz_context *ctx = pdev->ctx;
	gstate *gs = CURRENT_GSTATE(pdev);
	int i;

	pdf_dev_end_text(pdev);

	pdf_dict_puts_drop(pdev->contents, "Length", pdf_new_int(doc, gs->buf->len));

	for (i = pdev->num_gstates-1; i >= 0; i--)
	{
		fz_drop_stroke_state(ctx, pdev->gstates[i].stroke_state);
	}

	for (i = pdev->num_fonts-1; i >= 0; i--)
	{
		fz_drop_font(ctx, pdev->fonts[i].font);
	}

	for (i = pdev->num_imgs-1; i >= 0; i--)
	{
		pdf_drop_obj(pdev->images[i].ref);
	}

	pdf_update_stream(doc, pdf_to_num(pdev->contents), pdev->gstates[0].buf);
	fz_drop_buffer(ctx, pdev->gstates[0].buf);

	pdf_drop_obj(pdev->contents);
	pdf_drop_obj(pdev->resources);

	fz_free(ctx, pdev->images);
	fz_free(ctx, pdev->alphas);
	fz_free(ctx, pdev->gstates);
	fz_free(ctx, pdev);
}

fz_device *pdf_new_pdf_device(pdf_document *doc, pdf_obj *contents, pdf_obj *resources, const fz_matrix *ctm)
{
	fz_context *ctx = doc->ctx;
	pdf_device *pdev = fz_malloc_struct(ctx, pdf_device);
	fz_device *dev;

	fz_try(ctx)
	{
		pdev->ctx = ctx;
		pdev->doc = doc;
		pdev->contents = pdf_keep_obj(contents);
		pdev->resources = pdf_keep_obj(resources);
		pdev->gstates = fz_malloc_struct(ctx, gstate);
		pdev->gstates[0].buf = fz_new_buffer(ctx, 256);
		pdev->gstates[0].ctm = *ctm;
		pdev->gstates[0].colorspace[0] = fz_device_gray(ctx);
		pdev->gstates[0].colorspace[1] = fz_device_gray(ctx);
		pdev->gstates[0].color[0][0] = 1;
		pdev->gstates[0].color[1][0] = 1;
		pdev->gstates[0].alpha[0] = 1.0;
		pdev->gstates[0].alpha[1] = 1.0;
		pdev->gstates[0].font = -1;
		pdev->gstates[0].horizontal_scaling = 100;
		pdev->num_gstates = 1;
		pdev->max_gstates = 1;

		dev = fz_new_device(ctx, pdev);
	}
	fz_catch(ctx)
	{
		if (pdev->gstates)
			fz_drop_buffer(ctx, pdev->gstates[0].buf);
		fz_free(ctx, pdev);
		fz_rethrow(ctx);
	}

	dev->free_user = pdf_dev_free_user;

	dev->fill_path = pdf_dev_fill_path;
	dev->stroke_path = pdf_dev_stroke_path;
	dev->clip_path = pdf_dev_clip_path;
	dev->clip_stroke_path = pdf_dev_clip_stroke_path;

	dev->fill_text = pdf_dev_fill_text;
	dev->stroke_text = pdf_dev_stroke_text;
	dev->clip_text = pdf_dev_clip_text;
	dev->clip_stroke_text = pdf_dev_clip_stroke_text;
	dev->ignore_text = pdf_dev_ignore_text;

	dev->fill_shade = pdf_dev_fill_shade;
	dev->fill_image = pdf_dev_fill_image;
	dev->fill_image_mask = pdf_dev_fill_image_mask;
	dev->clip_image_mask = pdf_dev_clip_image_mask;

	dev->pop_clip = pdf_dev_pop_clip;

	dev->begin_mask = pdf_dev_begin_mask;
	dev->end_mask = pdf_dev_end_mask;
	dev->begin_group = pdf_dev_begin_group;
	dev->end_group = pdf_dev_end_group;

	dev->begin_tile = pdf_dev_begin_tile;
	dev->end_tile = pdf_dev_end_tile;

	return dev;
}

fz_device *pdf_page_write(pdf_document *doc, pdf_page *page)
{
	pdf_obj *resources = pdf_dict_gets(page->me, "Resources");
	fz_matrix ctm;
	fz_pre_translate(fz_scale(&ctm, 1, -1), 0, page->mediabox.y0-page->mediabox.y1);
	if (resources == NULL)
	{
		resources = pdf_new_dict(doc, 0);
		pdf_dict_puts_drop(page->me, "Resources", resources);
	}

	return pdf_new_pdf_device(doc, page->contents, resources, &ctm);
}
