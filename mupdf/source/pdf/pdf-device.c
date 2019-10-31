#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H

#define ALLOWED_TEXT_POS_ERROR (0.001f)

typedef struct pdf_device_s pdf_device;

typedef struct gstate_s gstate;

struct gstate_s
{
	/* The first few entries aren't really graphics state things, but
	 * they are recorded here as they are fundamentally intertwined with
	 * the push/pulling of the gstates. */
	fz_buffer *buf;
	void (*on_pop)(fz_context*,pdf_device*,void *);
	void *on_pop_arg;

	/* The graphics state proper */
	fz_matrix ctm;
	fz_colorspace *colorspace[2];
	float color[2][4];
	float alpha[2];
	fz_stroke_state *stroke_state;
	int font;
	float font_size;
	int text_rendering_mode;
	int knockout;
};

/* The image digest information, object reference, as well as indirect reference
 * ID are all stored in doc->resources->image, and so they are maintained
 * through the life of the document not just this page level device. As we
 * encounter images on a page, we will add to the hash table if they are not
 * already present.  When we have an image on a particular page, the resource
 * dict will be updated with the proper indirect reference across the document.
 * We do need to maintain some information as to what image resources we have
 * already specified for this page which is the purpose of image_indices
 */

typedef struct alpha_entry_s alpha_entry;

struct alpha_entry_s
{
	float alpha;
	int stroke;
};

typedef struct group_entry_s group_entry;

struct group_entry_s
{
	int alpha;
	int isolated;
	int knockout;
	fz_colorspace *colorspace;
	pdf_obj *ref;
};

struct pdf_device_s
{
	fz_device super;

	pdf_document *doc;
	pdf_obj *resources;

	int in_text;

	int num_forms;
	int num_smasks;

	int num_gstates;
	int max_gstates;
	gstate *gstates;

	int num_imgs;
	int max_imgs;
	int *image_indices;

	int num_cid_fonts;
	int max_cid_fonts;
	fz_font **cid_fonts;

	int num_alphas;
	int max_alphas;
	alpha_entry *alphas;

	int num_groups;
	int max_groups;
	group_entry *groups;
};

#define CURRENT_GSTATE(pdev) (&(pdev)->gstates[(pdev)->num_gstates-1])

/* Helper functions */
static void
pdf_dev_stroke_state(fz_context *ctx, pdf_device *pdev, const fz_stroke_state *stroke_state)
{
	gstate *gs = CURRENT_GSTATE(pdev);

	if (stroke_state == gs->stroke_state)
		return;
	if (gs->stroke_state && !memcmp(stroke_state, gs->stroke_state, sizeof(*stroke_state)))
		return;
	if (!gs->stroke_state || gs->stroke_state->linewidth != stroke_state->linewidth)
	{
		fz_append_printf(ctx, gs->buf, "%g w\n", stroke_state->linewidth);
	}
	if (!gs->stroke_state || gs->stroke_state->start_cap != stroke_state->start_cap)
	{
		int cap = stroke_state->start_cap;
		/* FIXME: Triangle caps aren't supported in pdf */
		if (cap == FZ_LINECAP_TRIANGLE)
			cap = FZ_LINECAP_BUTT;
		fz_append_printf(ctx, gs->buf, "%d J\n", cap);
	}
	if (!gs->stroke_state || gs->stroke_state->linejoin != stroke_state->linejoin)
	{
		int join = stroke_state->linejoin;
		if (join == FZ_LINEJOIN_MITER_XPS)
			join = FZ_LINEJOIN_MITER;
		fz_append_printf(ctx, gs->buf, "%d j\n", join);
	}
	if (!gs->stroke_state || gs->stroke_state->miterlimit != stroke_state->miterlimit)
	{
		fz_append_printf(ctx, gs->buf, "%g M\n", stroke_state->miterlimit);
	}
	if (gs->stroke_state == NULL && stroke_state->dash_len == 0)
	{}
	else if (!gs->stroke_state || gs->stroke_state->dash_phase != stroke_state->dash_phase || gs->stroke_state->dash_len != stroke_state->dash_len ||
		memcmp(gs->stroke_state->dash_list, stroke_state->dash_list, sizeof(float)*stroke_state->dash_len))
	{
		int i;
		fz_append_byte(ctx, gs->buf, '[');
		for (i = 0; i < stroke_state->dash_len; i++)
		{
			if (i > 0)
				fz_append_byte(ctx, gs->buf, ' ');
			fz_append_printf(ctx, gs->buf, "%g", stroke_state->dash_list[i]);
		}
		fz_append_printf(ctx, gs->buf, "]%g d\n", stroke_state->dash_phase);
	}
	fz_drop_stroke_state(ctx, gs->stroke_state);
	gs->stroke_state = fz_keep_stroke_state(ctx, stroke_state);
}

typedef struct
{
	fz_context *ctx;
	fz_buffer *buf;
} pdf_dev_path_arg;

static void
pdf_dev_path_moveto(fz_context *ctx, void *arg, float x, float y)
{
	fz_buffer *buf = (fz_buffer *)arg;
	fz_append_printf(ctx, buf, "%g %g m\n", x, y);
}

static void
pdf_dev_path_lineto(fz_context *ctx, void *arg, float x, float y)
{
	fz_buffer *buf = (fz_buffer *)arg;
	fz_append_printf(ctx, buf, "%g %g l\n", x, y);
}

static void
pdf_dev_path_curveto(fz_context *ctx, void *arg, float x1, float y1, float x2, float y2, float x3, float y3)
{
	fz_buffer *buf = (fz_buffer *)arg;
	fz_append_printf(ctx, buf, "%g %g %g %g %g %g c\n", x1, y1, x2, y2, x3, y3);
}

static void
pdf_dev_path_close(fz_context *ctx, void *arg)
{
	fz_buffer *buf = (fz_buffer *)arg;
	fz_append_string(ctx, buf, "h\n");
}

static const fz_path_walker pdf_dev_path_proc =
{
	pdf_dev_path_moveto,
	pdf_dev_path_lineto,
	pdf_dev_path_curveto,
	pdf_dev_path_close
};

static void
pdf_dev_path(fz_context *ctx, pdf_device *pdev, const fz_path *path)
{
	gstate *gs = CURRENT_GSTATE(pdev);

	fz_walk_path(ctx, path, &pdf_dev_path_proc, (void *)gs->buf);
}

static void
pdf_dev_ctm(fz_context *ctx, pdf_device *pdev, fz_matrix ctm)
{
	fz_matrix inverse;
	gstate *gs = CURRENT_GSTATE(pdev);

	if (memcmp(&gs->ctm, &ctm, sizeof(ctm)) == 0)
		return;
	inverse = fz_invert_matrix(gs->ctm);
	inverse = fz_concat(ctm, inverse);
	gs->ctm = ctm;
	fz_append_printf(ctx, gs->buf, "%M cm\n", &inverse);
}

static void
pdf_dev_color(fz_context *ctx, pdf_device *pdev, fz_colorspace *colorspace, const float *color, int stroke, fz_color_params color_params)
{
	int diff = 0;
	int i;
	int cspace = 0;
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
		fz_convert_color(ctx, colorspace, color, fz_device_rgb(ctx), rgb, NULL, color_params);
		color = rgb;
		colorspace = fz_device_rgb(ctx);
		cspace = 3;
	}

	if (gs->colorspace[stroke] != colorspace)
	{
		gs->colorspace[stroke] = colorspace;
		diff = 1;
	}

	for (i=0; i < cspace; i++)
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
			fz_append_printf(ctx, gs->buf, "%g g\n", color[0]);
			break;
		case 3:
			fz_append_printf(ctx, gs->buf, "%g %g %g rg\n", color[0], color[1], color[2]);
			break;
		case 4:
			fz_append_printf(ctx, gs->buf, "%g %g %g %g k\n", color[0], color[1], color[2], color[3]);
			break;
		case 1+8:
			fz_append_printf(ctx, gs->buf, "%g G\n", color[0]);
			break;
		case 3+8:
			fz_append_printf(ctx, gs->buf, "%g %g %g RG\n", color[0], color[1], color[2]);
			break;
		case 4+8:
			fz_append_printf(ctx, gs->buf, "%g %g %g %g K\n", color[0], color[1], color[2], color[3]);
			break;
	}
}

static void
pdf_dev_alpha(fz_context *ctx, pdf_device *pdev, float alpha, int stroke)
{
	int i;
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
		pdf_obj *o, *ref;

		/* No. Need to make a new one */
		if (pdev->num_alphas == pdev->max_alphas)
		{
			int newmax = pdev->max_alphas * 2;
			if (newmax == 0)
				newmax = 4;
			pdev->alphas = fz_realloc_array(ctx, pdev->alphas, newmax, alpha_entry);
			pdev->max_alphas = newmax;
		}
		pdev->alphas[i].alpha = alpha;
		pdev->alphas[i].stroke = stroke;

		o = pdf_new_dict(ctx, doc, 1);
		fz_try(ctx)
		{
			char text[32];
			pdf_dict_put_real(ctx, o, (stroke ? PDF_NAME(CA) : PDF_NAME(ca)), alpha);
			fz_snprintf(text, sizeof(text), "ExtGState/Alp%d", i);
			ref = pdf_add_object(ctx, doc, o);
			pdf_dict_putp_drop(ctx, pdev->resources, text, ref);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(ctx, o);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
		pdev->num_alphas++;
	}
	fz_append_printf(ctx, gs->buf, "/Alp%d gs\n", i);
}

static int
pdf_dev_add_font_res(fz_context *ctx, pdf_device *pdev, fz_font *font)
{
	pdf_obj *fres;
	char text[32];
	int k;
	int num;

	/* Check if we already had this one */
	for (k = 0; k < pdev->num_cid_fonts; k++)
		if (pdev->cid_fonts[k] == font)
			return k;

	/* This will add it to the xref if needed */
	fres = pdf_add_cid_font(ctx, pdev->doc, font);

	/* Not there so add to resources */
	fz_snprintf(text, sizeof(text), "Font/F%d", pdev->num_cid_fonts);
	pdf_dict_putp_drop(ctx, pdev->resources, text, fres);

	/* And add index to our list for this page */
	if (pdev->num_cid_fonts == pdev->max_cid_fonts)
	{
		int newmax = pdev->max_cid_fonts * 2;
		if (newmax == 0)
			newmax = 4;
		pdev->cid_fonts = fz_realloc_array(ctx, pdev->cid_fonts, newmax, fz_font*);
		pdev->max_cid_fonts = newmax;
	}
	num = pdev->num_cid_fonts++;
	pdev->cid_fonts[num] = fz_keep_font(ctx, font);
	return num;
}

static void
pdf_dev_font(fz_context *ctx, pdf_device *pdev, fz_font *font, fz_matrix trm)
{
	gstate *gs = CURRENT_GSTATE(pdev);
	float font_size = fz_matrix_expansion(trm);

	/* If the font is unchanged, nothing to do */
	if (gs->font >= 0 && pdev->cid_fonts[gs->font] == font && gs->font_size == font_size)
		return;

	if (fz_font_t3_procs(ctx, font))
		fz_throw(ctx, FZ_ERROR_GENERIC, "pdf device does not support type 3 fonts");
	if (fz_font_flags(font)->ft_substitute)
		fz_throw(ctx, FZ_ERROR_GENERIC, "pdf device does not support substitute fonts");
	if (!pdf_font_writing_supported(font))
		fz_throw(ctx, FZ_ERROR_GENERIC, "pdf device does not support font types found in this file");

	gs->font = pdf_dev_add_font_res(ctx, pdev, font);
	gs->font_size = font_size;

	fz_append_printf(ctx, gs->buf, "/F%d %g Tf\n", gs->font, gs->font_size);
}

static void
pdf_dev_push_new_buf(fz_context *ctx, pdf_device *pdev, fz_buffer *buf, void (*on_pop)(fz_context*,pdf_device*,void*), void *on_pop_arg)
{
	if (pdev->num_gstates == pdev->max_gstates)
	{
		int newmax = pdev->max_gstates*2;
		pdev->gstates = fz_realloc_array(ctx, pdev->gstates, newmax, gstate);
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
	fz_append_string(ctx, pdev->gstates[pdev->num_gstates].buf, "q\n");
	pdev->num_gstates++;
}

static void
pdf_dev_push(fz_context *ctx, pdf_device *pdev)
{
	pdf_dev_push_new_buf(ctx, pdev, NULL, NULL, NULL);
}

static void *
pdf_dev_pop(fz_context *ctx, pdf_device *pdev)
{
	gstate *gs = CURRENT_GSTATE(pdev);
	void *arg = gs->on_pop_arg;

	fz_append_string(ctx, gs->buf, "Q\n");
	if (gs->on_pop)
		gs->on_pop(ctx, pdev, arg);
	pdev->num_gstates--;
	fz_drop_stroke_state(ctx, pdev->gstates[pdev->num_gstates].stroke_state);
	fz_drop_buffer(ctx, pdev->gstates[pdev->num_gstates].buf);
	return arg;
}

static void
pdf_dev_text_span(fz_context *ctx, pdf_device *pdev, fz_text_span *span)
{
	gstate *gs = CURRENT_GSTATE(pdev);
	fz_matrix trm, tm, tlm, inv_trm, inv_tm;
	fz_matrix inv_tfs;
	fz_point d;
	float adv;
	int dx, dy;
	int i;

	if (span->len == 0)
		return;

	inv_tfs = fz_scale(1 / gs->font_size, 1 / gs->font_size);

	trm = span->trm;
	trm.e = span->items[0].x;
	trm.f = span->items[0].y;

	tm = fz_concat(inv_tfs, trm);
	tlm = tm;

	inv_tm = fz_invert_matrix(tm);
	inv_trm = fz_invert_matrix(trm);

	fz_append_printf(ctx, gs->buf, "%M Tm\n[<", &tm);

	for (i = 0; i < span->len; ++i)
	{
		fz_text_item *it = &span->items[i];
		if (it->gid < 0)
			continue;

		/* transform difference from expected pen position into font units. */
		d.x = it->x - trm.e;
		d.y = it->y - trm.f;
		d = fz_transform_vector(d, inv_trm);
		dx = (int)(d.x * 1000 + (d.x < 0 ? -0.5f : 0.5f));
		dy = (int)(d.y * 1000 + (d.y < 0 ? -0.5f : 0.5f));

		trm.e = it->x;
		trm.f = it->y;

		if (dx != 0 || dy != 0)
		{
			if (span->wmode == 0 && dy == 0)
				fz_append_printf(ctx, gs->buf, ">%d<", -dx);
			else if (span->wmode == 1 && dx == 0)
				fz_append_printf(ctx, gs->buf, ">%d<", -dy);
			else
			{
				/* Calculate offset from start of the previous line */
				tm = fz_concat(inv_tfs, trm);
				d.x = tm.e - tlm.e;
				d.y = tm.f - tlm.f;
				d = fz_transform_vector(d, inv_tm);
				fz_append_printf(ctx, gs->buf, ">]TJ\n%g %g Td\n[<", d.x, d.y);
				tlm = tm;
			}
		}

		if (fz_font_t3_procs(ctx, span->font))
			fz_append_printf(ctx, gs->buf, "%02x", it->gid);
		else
			fz_append_printf(ctx, gs->buf, "%04x", it->gid);

		adv = fz_advance_glyph(ctx, span->font, it->gid, span->wmode);
		if (span->wmode == 0)
			trm = fz_pre_translate(trm, adv, 0);
		else
			trm = fz_pre_translate(trm, 0, adv);
	}

	fz_append_string(ctx, gs->buf, ">]TJ\n");
}

static void
pdf_dev_trm(fz_context *ctx, pdf_device *pdev, int trm)
{
	gstate *gs = CURRENT_GSTATE(pdev);

	if (gs->text_rendering_mode == trm)
		return;
	gs->text_rendering_mode = trm;
	fz_append_printf(ctx, gs->buf, "%d Tr\n", trm);
}

static void
pdf_dev_begin_text(fz_context *ctx, pdf_device *pdev, int trm)
{
	pdf_dev_trm(ctx, pdev, trm);
	if (!pdev->in_text)
	{
		gstate *gs = CURRENT_GSTATE(pdev);
		fz_append_string(ctx, gs->buf, "BT\n");
		pdev->in_text = 1;
	}
}

static void
pdf_dev_end_text(fz_context *ctx, pdf_device *pdev)
{
	gstate *gs = CURRENT_GSTATE(pdev);

	if (!pdev->in_text)
		return;
	pdev->in_text = 0;
	fz_append_string(ctx, gs->buf, "ET\n");
}

static int
pdf_dev_new_form(fz_context *ctx, pdf_obj **form_ref, pdf_device *pdev, fz_rect bbox, int isolated, int knockout, float alpha, fz_colorspace *colorspace)
{
	pdf_document *doc = pdev->doc;
	int num;
	pdf_obj *group_ref = NULL;
	pdf_obj *group;
	pdf_obj *form;

	*form_ref = NULL;

	/* Find (or make) a new group with the required options. */
	for(num = 0; num < pdev->num_groups; num++)
	{
		group_entry *g = &pdev->groups[num];
		if (g->isolated == isolated && g->knockout == knockout && g->alpha == alpha && g->colorspace == colorspace)
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
			pdev->groups = fz_realloc_array(ctx, pdev->groups, newmax, group_entry);
			pdev->max_groups = newmax;
		}
		pdev->num_groups++;
		pdev->groups[num].isolated = isolated;
		pdev->groups[num].knockout = knockout;
		pdev->groups[num].alpha = alpha;
		pdev->groups[num].colorspace = fz_keep_colorspace(ctx, colorspace);
		pdev->groups[num].ref = NULL;
		group = pdf_new_dict(ctx, doc, 5);
		fz_try(ctx)
		{
			pdf_dict_put(ctx, group, PDF_NAME(Type), PDF_NAME(Group));
			pdf_dict_put(ctx, group, PDF_NAME(S), PDF_NAME(Transparency));
			pdf_dict_put_bool(ctx, group, PDF_NAME(K), knockout);
			pdf_dict_put_bool(ctx, group, PDF_NAME(I), isolated);
			switch (fz_colorspace_type(ctx, colorspace))
			{
			case FZ_COLORSPACE_GRAY:
				pdf_dict_put(ctx, group, PDF_NAME(CS), PDF_NAME(DeviceGray));
				break;
			case FZ_COLORSPACE_RGB:
				pdf_dict_put(ctx, group, PDF_NAME(CS), PDF_NAME(DeviceRGB));
				break;
			case FZ_COLORSPACE_CMYK:
				pdf_dict_put(ctx, group, PDF_NAME(CS), PDF_NAME(DeviceCMYK));
				break;
			default:
				break;
			}
			group_ref = pdev->groups[num].ref = pdf_add_object(ctx, doc, group);
		}
		fz_always(ctx)
		{
			pdf_drop_obj(ctx, group);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
	}

	/* Make us a new Forms object that points to that group, and change
	 * to writing into the buffer for that Forms object. */
	form = pdf_new_dict(ctx, doc, 4);
	fz_try(ctx)
	{
		pdf_dict_put(ctx, form, PDF_NAME(Subtype), PDF_NAME(Form));
		pdf_dict_put(ctx, form, PDF_NAME(Group), group_ref);
		pdf_dict_put_int(ctx, form, PDF_NAME(FormType), 1);
		pdf_dict_put_rect(ctx, form, PDF_NAME(BBox), bbox);
		*form_ref = pdf_add_object(ctx, doc, form);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, form);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	/* Insert the new form object into the resources */
	{
		char text[32];
		num = pdev->num_forms++;
		fz_snprintf(text, sizeof(text), "XObject/Fm%d", num);
		pdf_dict_putp(ctx, pdev->resources, text, *form_ref);
	}

	return num;
}

/* Entry points */

static void
pdf_dev_fill_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	pdf_device *pdev = (pdf_device*)dev;
	gstate *gs = CURRENT_GSTATE(pdev);

	pdf_dev_end_text(ctx, pdev);
	pdf_dev_alpha(ctx, pdev, alpha, 0);
	pdf_dev_color(ctx, pdev, colorspace, color, 0, color_params);
	pdf_dev_ctm(ctx, pdev, ctm);
	pdf_dev_path(ctx, pdev, path);
	fz_append_string(ctx, gs->buf, (even_odd ? "f*\n" : "f\n"));
}

static void
pdf_dev_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm,
	fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	pdf_device *pdev = (pdf_device*)dev;
	gstate *gs = CURRENT_GSTATE(pdev);

	pdf_dev_end_text(ctx, pdev);
	pdf_dev_alpha(ctx, pdev, alpha, 1);
	pdf_dev_color(ctx, pdev, colorspace, color, 1, color_params);
	pdf_dev_ctm(ctx, pdev, ctm);
	pdf_dev_stroke_state(ctx, pdev, stroke);
	pdf_dev_path(ctx, pdev, path);
	fz_append_string(ctx, gs->buf, "S\n");
}

static void
pdf_dev_clip_path(fz_context *ctx, fz_device *dev, const fz_path *path, int even_odd, fz_matrix ctm, fz_rect scissor)
{
	pdf_device *pdev = (pdf_device*)dev;
	gstate *gs;

	pdf_dev_end_text(ctx, pdev);
	pdf_dev_push(ctx, pdev);
	pdf_dev_ctm(ctx, pdev, ctm);
	pdf_dev_path(ctx, pdev, path);
	gs = CURRENT_GSTATE(pdev);
	fz_append_string(ctx, gs->buf, (even_odd ? "W* n\n" : "W n\n"));
}

static void
pdf_dev_clip_stroke_path(fz_context *ctx, fz_device *dev, const fz_path *path, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	pdf_device *pdev = (pdf_device*)dev;
	gstate *gs;

	pdf_dev_end_text(ctx, pdev);
	pdf_dev_push(ctx, pdev);
	/* FIXME: Need to push a group, select a pattern (or shading) here,
	 * stroke with the pattern/shading. Then move to defining that pattern
	 * with the next calls to the device interface until the next pop
	 * when we pop the group. */
	pdf_dev_ctm(ctx, pdev, ctm);
	pdf_dev_path(ctx, pdev, path);
	gs = CURRENT_GSTATE(pdev);
	fz_append_string(ctx, gs->buf, "W n\n");
}

static void
pdf_dev_fill_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm,
		fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	pdf_device *pdev = (pdf_device*)dev;
	fz_text_span *span;

	pdf_dev_ctm(ctx, pdev, ctm);
	pdf_dev_alpha(ctx, pdev, alpha, 0);
	pdf_dev_color(ctx, pdev, colorspace, color, 0, color_params);

	for (span = text->head; span; span = span->next)
	{
		pdf_dev_begin_text(ctx, pdev, 0);
		pdf_dev_font(ctx, pdev, span->font, span->trm);
		pdf_dev_text_span(ctx, pdev, span);
	}
}

static void
pdf_dev_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm,
		fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	pdf_device *pdev = (pdf_device*)dev;
	fz_text_span *span;

	pdf_dev_ctm(ctx, pdev, ctm);
	pdf_dev_alpha(ctx, pdev, alpha, 1);
	pdf_dev_color(ctx, pdev, colorspace, color, 1, color_params);

	for (span = text->head; span; span = span->next)
	{
		pdf_dev_begin_text(ctx, pdev, 1);
		pdf_dev_font(ctx, pdev, span->font, span->trm);
		pdf_dev_text_span(ctx, pdev, span);
	}
}

static void
pdf_dev_clip_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm, fz_rect scissor)
{
	pdf_device *pdev = (pdf_device*)dev;
	fz_text_span *span;

	pdf_dev_end_text(ctx, pdev);
	pdf_dev_push(ctx, pdev);

	pdf_dev_ctm(ctx, pdev, ctm);

	for (span = text->head; span; span = span->next)
	{
		pdf_dev_begin_text(ctx, pdev, 7);
		pdf_dev_font(ctx, pdev, span->font, span->trm);
		pdf_dev_text_span(ctx, pdev, span);
	}
}

static void
pdf_dev_clip_stroke_text(fz_context *ctx, fz_device *dev, const fz_text *text, const fz_stroke_state *stroke, fz_matrix ctm, fz_rect scissor)
{
	pdf_device *pdev = (pdf_device*)dev;
	fz_text_span *span;

	pdf_dev_end_text(ctx, pdev);
	pdf_dev_push(ctx, pdev);

	pdf_dev_ctm(ctx, pdev, ctm);

	for (span = text->head; span; span = span->next)
	{
		pdf_dev_begin_text(ctx, pdev, 7);
		pdf_dev_font(ctx, pdev, span->font, span->trm);
		pdf_dev_text_span(ctx, pdev, span);
	}
}

static void
pdf_dev_ignore_text(fz_context *ctx, fz_device *dev, const fz_text *text, fz_matrix ctm)
{
	pdf_device *pdev = (pdf_device*)dev;
	fz_text_span *span;

	pdf_dev_ctm(ctx, pdev, ctm);

	for (span = text->head; span; span = span->next)
	{
		pdf_dev_begin_text(ctx, pdev, 0);
		pdf_dev_font(ctx, pdev, span->font, span->trm);
		pdf_dev_text_span(ctx, pdev, span);
	}
}

static void
pdf_dev_add_image_res(fz_context *ctx, fz_device *dev, pdf_obj *im_res)
{
	char text[32];
	pdf_device *pdev = (pdf_device*)dev;
	int k;
	int num;

	/* Check if we already had this one */
	for (k = 0; k < pdev->num_imgs; k++)
	{
		if (pdev->image_indices[k] == pdf_to_num(ctx, im_res))
			return;
	}

	/* Not there so add to resources */
	fz_snprintf(text, sizeof(text), "XObject/Img%d", pdf_to_num(ctx, im_res));
	pdf_dict_putp(ctx, pdev->resources, text, im_res);

	/* And add index to our list for this page */
	if (pdev->num_imgs == pdev->max_imgs)
	{
		int newmax = pdev->max_imgs * 2;
		if (newmax == 0)
			newmax = 4;
		pdev->image_indices = fz_realloc_array(ctx, pdev->image_indices, newmax, int);
		pdev->max_imgs = newmax;
	}
	num = pdev->num_imgs++;
	pdev->image_indices[num] = pdf_to_num(ctx, im_res);
}

static void
pdf_dev_fill_image(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	pdf_device *pdev = (pdf_device*)dev;
	pdf_obj *im_res;
	gstate *gs = CURRENT_GSTATE(pdev);

	pdf_dev_end_text(ctx, pdev);
	im_res = pdf_add_image(ctx, pdev->doc, image);
	if (im_res == NULL)
	{
		fz_warn(ctx, "pdf_add_image: problem adding image resource");
		return;
	}

	fz_try(ctx)
	{
		pdf_dev_alpha(ctx, pdev, alpha, 0);

		/* PDF images are upside down, so fiddle the ctm */
		ctm = fz_pre_scale(ctm, 1, -1);
		ctm = fz_pre_translate(ctm, 0, -1);
		pdf_dev_ctm(ctx, pdev, ctm);
		fz_append_printf(ctx, gs->buf, "/Img%d Do\n", pdf_to_num(ctx, im_res));

		/* Possibly add to page resources */
		pdf_dev_add_image_res(ctx, dev, im_res);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, im_res);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_dev_fill_shade(fz_context *ctx, fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha, fz_color_params color_params)
{
	pdf_device *pdev = (pdf_device*)dev;

	/* FIXME */
	pdf_dev_end_text(ctx, pdev);
}

static void
pdf_dev_fill_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm,
		fz_colorspace *colorspace, const float *color, float alpha, fz_color_params color_params)
{
	pdf_device *pdev = (pdf_device*)dev;
	pdf_obj *im_res = NULL;
	gstate *gs = CURRENT_GSTATE(pdev);

	pdf_dev_end_text(ctx, pdev);
	im_res = pdf_add_image(ctx, pdev->doc, image);
	if (im_res == NULL)
	{
		fz_warn(ctx, "pdf_add_image: problem adding image resource");
		return;
	}

	fz_try(ctx)
	{
		fz_append_string(ctx, gs->buf, "q\n");
		pdf_dev_alpha(ctx, pdev, alpha, 0);
		pdf_dev_color(ctx, pdev, colorspace, color, 0, color_params);

		/* PDF images are upside down, so fiddle the ctm */
		ctm = fz_pre_scale(ctm, 1, -1);
		ctm = fz_pre_translate(ctm, 0, -1);
		pdf_dev_ctm(ctx, pdev, ctm);
		fz_append_printf(ctx, gs->buf, "/Img%d Do Q\n", pdf_to_num(ctx, im_res));

		/* Possibly add to page resources */
		pdf_dev_add_image_res(ctx, dev, im_res);
	}
	fz_always(ctx)
		pdf_drop_obj(ctx, im_res);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

static void
pdf_dev_clip_image_mask(fz_context *ctx, fz_device *dev, fz_image *image, fz_matrix ctm, fz_rect scissor)
{
	pdf_device *pdev = (pdf_device*)dev;

	/* FIXME */
	pdf_dev_end_text(ctx, pdev);
	pdf_dev_push(ctx, pdev);
}

static void
pdf_dev_pop_clip(fz_context *ctx, fz_device *dev)
{
	pdf_device *pdev = (pdf_device*)dev;

	/* FIXME */
	pdf_dev_end_text(ctx, pdev);
	pdf_dev_pop(ctx, pdev);
}

static void
pdf_dev_begin_mask(fz_context *ctx, fz_device *dev, fz_rect bbox, int luminosity, fz_colorspace *colorspace, const float *color, fz_color_params color_params)
{
	pdf_device *pdev = (pdf_device*)dev;
	gstate *gs;
	pdf_obj *smask = NULL;
	char egsname[32];
	pdf_obj *egs = NULL;
	pdf_obj *egss;
	pdf_obj *form_ref;
	pdf_obj *color_obj = NULL;
	int i, n;

	fz_var(smask);
	fz_var(egs);
	fz_var(color_obj);

	pdf_dev_end_text(ctx, pdev);

	/* Make a new form to contain the contents of the softmask */
	pdf_dev_new_form(ctx, &form_ref, pdev, bbox, 0, 0, 1, colorspace);

	fz_try(ctx)
	{
		fz_snprintf(egsname, sizeof(egsname), "SM%d", pdev->num_smasks++);
		egss = pdf_dict_get(ctx, pdev->resources, PDF_NAME(ExtGState));
		if (!egss)
			egss = pdf_dict_put_dict(ctx, pdev->resources, PDF_NAME(ExtGState), 10);
		egs = pdf_dict_puts_dict(ctx, egss, egsname, 1);

		pdf_dict_put(ctx, egs, PDF_NAME(Type), PDF_NAME(ExtGState));
		smask = pdf_dict_put_dict(ctx, egs, PDF_NAME(SMask), 4);

		pdf_dict_put(ctx, smask, PDF_NAME(Type), PDF_NAME(Mask));
		pdf_dict_put(ctx, smask, PDF_NAME(S), (luminosity ? PDF_NAME(Luminosity) : PDF_NAME(Alpha)));
		pdf_dict_put(ctx, smask, PDF_NAME(G), form_ref);

		n = fz_colorspace_n(ctx, colorspace);
		color_obj = pdf_dict_put_array(ctx, smask, PDF_NAME(BC), n);
		for (i = 0; i < n; i++)
			pdf_array_push_real(ctx, color_obj, color[i]);

		gs = CURRENT_GSTATE(pdev);
		fz_append_printf(ctx, gs->buf, "/SM%d gs\n", pdev->num_smasks-1);
	}
	fz_catch(ctx)
	{
		pdf_drop_obj(ctx, form_ref);
		fz_rethrow(ctx);
	}

	/* Now, everything we get until the end_mask needs to go into a
	 * new buffer, which will be the stream contents for the form. */
	pdf_dev_push_new_buf(ctx, pdev, fz_new_buffer(ctx, 1024), NULL, form_ref);
}

static void
pdf_dev_end_mask(fz_context *ctx, fz_device *dev)
{
	pdf_device *pdev = (pdf_device*)dev;
	pdf_document *doc = pdev->doc;
	gstate *gs = CURRENT_GSTATE(pdev);
	pdf_obj *form_ref = (pdf_obj *)gs->on_pop_arg;

	/* Here we do part of the pop, but not all of it. */
	pdf_dev_end_text(ctx, pdev);
	fz_append_string(ctx, gs->buf, "Q\n");
	pdf_update_stream(ctx, doc, form_ref, gs->buf, 0);
	fz_drop_buffer(ctx, gs->buf);
	gs->buf = fz_keep_buffer(ctx, gs[-1].buf);
	gs->on_pop_arg = NULL;
	pdf_drop_obj(ctx, form_ref);
	fz_append_string(ctx, gs->buf, "q\n");
}

static void
pdf_dev_begin_group(fz_context *ctx, fz_device *dev, fz_rect bbox, fz_colorspace *cs, int isolated, int knockout, int blendmode, float alpha)
{
	pdf_device *pdev = (pdf_device*)dev;
	pdf_document *doc = pdev->doc;
	int num;
	pdf_obj *form_ref;
	gstate *gs;

	pdf_dev_end_text(ctx, pdev);

	num = pdf_dev_new_form(ctx, &form_ref, pdev, bbox, isolated, knockout, alpha, cs);

	/* Do we have an appropriate blending extgstate already? */
	{
		char text[32];
		pdf_obj *obj;
		fz_snprintf(text, sizeof(text), "ExtGState/BlendMode%d", blendmode);
		obj = pdf_dict_getp(ctx, pdev->resources, text);
		if (obj == NULL)
		{
			/* No, better make one */
			obj = pdf_new_dict(ctx, doc, 2);
			pdf_dict_put(ctx, obj, PDF_NAME(Type), PDF_NAME(ExtGState));
			pdf_dict_put_name(ctx, obj, PDF_NAME(BM), fz_blendmode_name(blendmode));
			pdf_dict_putp_drop(ctx, pdev->resources, text, obj);
		}
	}

	/* Add the call to this group */
	gs = CURRENT_GSTATE(pdev);
	fz_append_printf(ctx, gs->buf, "/BlendMode%d gs /Fm%d Do\n", blendmode, num);

	/* Now, everything we get until the end of group needs to go into a
	 * new buffer, which will be the stream contents for the form. */
	pdf_dev_push_new_buf(ctx, pdev, fz_new_buffer(ctx, 1024), NULL, form_ref);
}

static void
pdf_dev_end_group(fz_context *ctx, fz_device *dev)
{
	pdf_device *pdev = (pdf_device*)dev;
	pdf_document *doc = pdev->doc;
	gstate *gs = CURRENT_GSTATE(pdev);
	fz_buffer *buf = fz_keep_buffer(ctx, gs->buf);
	pdf_obj *form_ref;

	pdf_dev_end_text(ctx, pdev);
	form_ref = (pdf_obj *)pdf_dev_pop(ctx, pdev);
	pdf_update_stream(ctx, doc, form_ref, buf, 0);
	fz_drop_buffer(ctx, buf);
	pdf_drop_obj(ctx, form_ref);
}

static int
pdf_dev_begin_tile(fz_context *ctx, fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm, int id)
{
	pdf_device *pdev = (pdf_device*)dev;

	/* FIXME */
	pdf_dev_end_text(ctx, pdev);
	return 0;
}

static void
pdf_dev_end_tile(fz_context *ctx, fz_device *dev)
{
	pdf_device *pdev = (pdf_device*)dev;

	/* FIXME */
	pdf_dev_end_text(ctx, pdev);
}

static void
pdf_dev_close_device(fz_context *ctx, fz_device *dev)
{
	pdf_device *pdev = (pdf_device*)dev;
	pdf_dev_end_text(ctx, pdev);
}

static void
pdf_dev_drop_device(fz_context *ctx, fz_device *dev)
{
	pdf_device *pdev = (pdf_device*)dev;
	int i;

	for (i = pdev->num_gstates-1; i >= 0; i--)
	{
		fz_drop_buffer(ctx, pdev->gstates[i].buf);
		fz_drop_stroke_state(ctx, pdev->gstates[i].stroke_state);
	}

	for (i = pdev->num_cid_fonts-1; i >= 0; i--)
		fz_drop_font(ctx, pdev->cid_fonts[i]);

	for (i = pdev->num_groups - 1; i >= 0; i--)
	{
		pdf_drop_obj(ctx, pdev->groups[i].ref);
		fz_drop_colorspace(ctx, pdev->groups[i].colorspace);
	}

	pdf_drop_obj(ctx, pdev->resources);
	fz_free(ctx, pdev->cid_fonts);
	fz_free(ctx, pdev->image_indices);
	fz_free(ctx, pdev->groups);
	fz_free(ctx, pdev->alphas);
	fz_free(ctx, pdev->gstates);
}

/*
	Create a pdf device. Rendering to the device creates
	new pdf content. WARNING: this device is work in progress. It doesn't
	currently support all rendering cases.

	Note that contents must be a stream (dictionary) to be updated (or
	a reference to a stream). Callers should take care to ensure that it
	is not an array, and that is it not shared with other objects/pages.
*/
fz_device *pdf_new_pdf_device(fz_context *ctx, pdf_document *doc, fz_matrix topctm, fz_rect mediabox, pdf_obj *resources, fz_buffer *buf)
{
	pdf_device *dev = fz_new_derived_device(ctx, pdf_device);

	dev->super.close_device = pdf_dev_close_device;
	dev->super.drop_device = pdf_dev_drop_device;

	dev->super.fill_path = pdf_dev_fill_path;
	dev->super.stroke_path = pdf_dev_stroke_path;
	dev->super.clip_path = pdf_dev_clip_path;
	dev->super.clip_stroke_path = pdf_dev_clip_stroke_path;

	dev->super.fill_text = pdf_dev_fill_text;
	dev->super.stroke_text = pdf_dev_stroke_text;
	dev->super.clip_text = pdf_dev_clip_text;
	dev->super.clip_stroke_text = pdf_dev_clip_stroke_text;
	dev->super.ignore_text = pdf_dev_ignore_text;

	dev->super.fill_shade = pdf_dev_fill_shade;
	dev->super.fill_image = pdf_dev_fill_image;
	dev->super.fill_image_mask = pdf_dev_fill_image_mask;
	dev->super.clip_image_mask = pdf_dev_clip_image_mask;

	dev->super.pop_clip = pdf_dev_pop_clip;

	dev->super.begin_mask = pdf_dev_begin_mask;
	dev->super.end_mask = pdf_dev_end_mask;
	dev->super.begin_group = pdf_dev_begin_group;
	dev->super.end_group = pdf_dev_end_group;

	dev->super.begin_tile = pdf_dev_begin_tile;
	dev->super.end_tile = pdf_dev_end_tile;

	fz_var(buf);

	fz_try(ctx)
	{
		if (buf)
			buf = fz_keep_buffer(ctx, buf);
		else
			buf = fz_new_buffer(ctx, 256);
		dev->doc = doc;
		dev->resources = pdf_keep_obj(ctx, resources);
		dev->gstates = fz_malloc_struct(ctx, gstate);
		dev->gstates[0].buf = buf;
		dev->gstates[0].ctm = fz_identity; // XXX
		dev->gstates[0].colorspace[0] = fz_device_gray(ctx);
		dev->gstates[0].colorspace[1] = fz_device_gray(ctx);
		dev->gstates[0].color[0][0] = 1;
		dev->gstates[0].color[1][0] = 1;
		dev->gstates[0].alpha[0] = 1.0f;
		dev->gstates[0].alpha[1] = 1.0f;
		dev->gstates[0].font = -1;
		dev->num_gstates = 1;
		dev->max_gstates = 1;

		if (!fz_is_identity(topctm))
			fz_append_printf(ctx, buf, "%M cm\n", &topctm);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, buf);
		fz_free(ctx, dev);
		fz_rethrow(ctx);
	}

	return (fz_device*)dev;
}

/*
	Create a device that will record the
	graphical operations given to it into a sequence of
	pdf operations, together with a set of resources. This
	sequence/set pair can then be used as the basis for
	adding a page to the document (see pdf_add_page).

	doc: The document for which these are intended.

	mediabox: The bbox for the created page.

	presources: Pointer to a place to put the created
	resources dictionary.

	pcontents: Pointer to a place to put the created
	contents buffer.
*/
fz_device *pdf_page_write(fz_context *ctx, pdf_document *doc, fz_rect mediabox, pdf_obj **presources, fz_buffer **pcontents)
{
	fz_matrix pagectm = { 1, 0, 0, -1, -mediabox.x0, mediabox.y1 };
	*presources = pdf_new_dict(ctx, doc, 0);
	*pcontents = fz_new_buffer(ctx, 0);
	return pdf_new_pdf_device(ctx, doc, pagectm, mediabox, *presources, *pcontents);
}
