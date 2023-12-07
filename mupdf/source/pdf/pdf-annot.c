// Copyright (C) 2004-2023 Artifex Software, Inc.
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
#include "pdf-annot-imp.h"

#include <string.h>

pdf_annot *
pdf_keep_annot(fz_context *ctx, pdf_annot *annot)
{
	return fz_keep_imp(ctx, annot, &annot->refs);
}

void
pdf_drop_annot(fz_context *ctx, pdf_annot *annot)
{
	if (fz_drop_imp(ctx, annot, &annot->refs))
	{
		pdf_drop_obj(ctx, annot->obj);
		fz_free(ctx, annot);
	}
}

void
pdf_drop_annots(fz_context *ctx, pdf_annot *annot)
{
	while (annot)
	{
		pdf_annot *next = annot->next;
		pdf_drop_annot(ctx, annot);
		annot = next;
	}
}

pdf_obj *
pdf_annot_ap(fz_context *ctx, pdf_annot *annot)
{
	int flags = pdf_dict_get_int(ctx, annot->obj, PDF_NAME(F));
	int readonly = flags & PDF_ANNOT_IS_READ_ONLY;

	pdf_obj *ap = pdf_dict_get(ctx, annot->obj, PDF_NAME(AP));
	pdf_obj *ap_n = pdf_dict_get(ctx, ap, PDF_NAME(N));
	pdf_obj *ap_r = pdf_dict_get(ctx, ap, PDF_NAME(R));
	pdf_obj *ap_d = pdf_dict_get(ctx, ap, PDF_NAME(D));

	if (!readonly && annot->is_hot && annot->is_active && ap_d)
		ap = ap_d;
	else if (!readonly && annot->is_hot && ap_r)
		ap = ap_r;
	else
		ap = ap_n;

	/* AP/N, AP/R and AP/D may be streams, or dictionaries of streams. */

	/* If it's a stream, we have a winner! */
	if (pdf_is_indirect(ctx, ap) && pdf_obj_num_is_stream(ctx, annot->page->doc, pdf_to_num(ctx, ap)))
		return ap;

	/* If it's not a stream, it may be a dictionary containing
	 * a range of possible values, that should be indexed by
	 * AS. */
	return pdf_dict_get(ctx, ap, pdf_dict_get(ctx, annot->obj, PDF_NAME(AS)));
}

int pdf_annot_active(fz_context *ctx, pdf_annot *annot)
{
	return annot ? annot->is_active : 0;
}

void pdf_set_annot_active(fz_context *ctx, pdf_annot *annot, int active)
{
	int old;

	if (!annot)
		return;

	old = annot->is_active;
	annot->is_active = !!active;
	if (old != annot->is_active)
		pdf_set_annot_has_changed(ctx, annot);
}

int pdf_annot_hot(fz_context *ctx, pdf_annot *annot)
{
	return annot ? annot->is_hot : 0;
}

void pdf_set_annot_hot(fz_context *ctx, pdf_annot *annot, int hot)
{
	int old;

	if (!annot)
		return;

	old = annot->is_hot;
	annot->is_hot = !!hot;
	if (old != annot->is_hot)
		pdf_set_annot_has_changed(ctx, annot);
}

fz_matrix
pdf_annot_transform(fz_context *ctx, pdf_annot *annot)
{
	fz_rect bbox, rect;
	fz_matrix matrix;
	float w, h, x, y;
	pdf_obj *ap = pdf_annot_ap(ctx, annot);

	rect = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
	bbox = pdf_xobject_bbox(ctx, ap);
	matrix = pdf_xobject_matrix(ctx, ap);

	bbox = fz_transform_rect(bbox, matrix);
	if (bbox.x1 == bbox.x0)
		w = 0;
	else
		w = (rect.x1 - rect.x0) / (bbox.x1 - bbox.x0);
	if (bbox.y1 == bbox.y0)
		h = 0;
	else
		h = (rect.y1 - rect.y0) / (bbox.y1 - bbox.y0);
	x = rect.x0 - (bbox.x0 * w);
	y = rect.y0 - (bbox.y0 * h);

	return fz_pre_scale(fz_translate(x, y), w, h);
}

/*
	Internal function for creating a new pdf annotation.
*/
static pdf_annot *
pdf_new_annot(fz_context *ctx, pdf_page *page, pdf_obj *obj)
{
	pdf_annot *annot;

	annot = fz_malloc_struct(ctx, pdf_annot);
	annot->refs = 1;
	annot->page = page; /* only borrowed, as the page owns the annot */
	annot->obj = pdf_keep_obj(ctx, obj);

	return annot;
}

void
pdf_load_annots(fz_context *ctx, pdf_page *page, pdf_obj *annots)
{
	pdf_annot *annot;
	pdf_obj *subtype;
	int i, n;

	n = pdf_array_len(ctx, annots);
	for (i = 0; i < n; ++i)
	{
		pdf_obj *obj = pdf_array_get(ctx, annots, i);
		if (pdf_is_dict(ctx, obj))
		{
			subtype = pdf_dict_get(ctx, obj, PDF_NAME(Subtype));
			if (pdf_name_eq(ctx, subtype, PDF_NAME(Link)))
				continue;
			if (pdf_name_eq(ctx, subtype, PDF_NAME(Popup)))
				continue;

			annot = pdf_new_annot(ctx, page, obj);
			if (pdf_name_eq(ctx, subtype, PDF_NAME(Widget)))
			{
				*page->widget_tailp = annot;
				page->widget_tailp = &annot->next;
			}
			else
			{
				*page->annot_tailp = annot;
				page->annot_tailp = &annot->next;
			}
		}
	}

	/* We need to run a resynth pass on the annotations on this
	 * page. That means rerunning it on the complete document. */
	page->doc->resynth_required = 1;
	/* And actually update the page so that any annotations required
	 * get synthesised. */
	pdf_update_page(ctx, page);
}

pdf_annot *
pdf_first_annot(fz_context *ctx, pdf_page *page)
{
	return page ? page->annots : NULL;
}

pdf_annot *
pdf_next_annot(fz_context *ctx, pdf_annot *annot)
{
	return annot ? annot->next : NULL;
}

pdf_obj *pdf_annot_obj(fz_context *ctx, pdf_annot *annot)
{
	return annot ? annot->obj : NULL;
}

pdf_page *pdf_annot_page(fz_context *ctx, pdf_annot *annot)
{
	return annot ? annot->page : NULL;
}

fz_rect
pdf_bound_annot(fz_context *ctx, pdf_annot *annot)
{
	fz_matrix page_ctm;
	fz_rect rect;
	int flags;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		rect = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);

		flags = pdf_dict_get_int(ctx, annot->obj, PDF_NAME(F));
		if (flags & PDF_ANNOT_IS_NO_ROTATE)
		{
			int rotate = pdf_dict_get_inheritable_int(ctx, annot->page->obj, PDF_NAME(Rotate));
			fz_point tp = fz_transform_point_xy(rect.x0, rect.y1, page_ctm);
			page_ctm = fz_concat(page_ctm, fz_translate(-tp.x, -tp.y));
			page_ctm = fz_concat(page_ctm, fz_rotate(-rotate));
			page_ctm = fz_concat(page_ctm, fz_translate(tp.x, tp.y));
		}
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return fz_transform_rect(rect, page_ctm);
}

void
pdf_annot_request_resynthesis(fz_context *ctx, pdf_annot *annot)
{
	if (annot == NULL)
		return;

	annot->needs_new_ap = 1;
	annot->page->doc->resynth_required = 1;
}

void
pdf_annot_request_synthesis(fz_context *ctx, pdf_annot *annot)
{
	if (annot == NULL)
		return;
	if (!pdf_annot_ap(ctx, annot))
		pdf_annot_request_resynthesis(ctx, annot);
}

int
pdf_annot_needs_resynthesis(fz_context *ctx, pdf_annot *annot)
{
	return annot ? annot->needs_new_ap : 0;
}

void pdf_set_annot_resynthesised(fz_context *ctx, pdf_annot *annot)
{
	if (annot == NULL)
		return;

	annot->needs_new_ap = 0;
	pdf_set_annot_has_changed(ctx, annot);
}

void pdf_set_annot_has_changed(fz_context *ctx, pdf_annot *annot)
{
	if (annot == NULL)
		return;

	annot->has_new_ap = 1;
}

void
pdf_dirty_annot(fz_context *ctx, pdf_annot *annot)
{
	pdf_annot_request_resynthesis(ctx, annot);
}

const char *
pdf_string_from_annot_type(fz_context *ctx, enum pdf_annot_type type)
{
	switch (type)
	{
	case PDF_ANNOT_TEXT: return "Text";
	case PDF_ANNOT_LINK: return "Link";
	case PDF_ANNOT_FREE_TEXT: return "FreeText";
	case PDF_ANNOT_LINE: return "Line";
	case PDF_ANNOT_SQUARE: return "Square";
	case PDF_ANNOT_CIRCLE: return "Circle";
	case PDF_ANNOT_POLYGON: return "Polygon";
	case PDF_ANNOT_POLY_LINE: return "PolyLine";
	case PDF_ANNOT_HIGHLIGHT: return "Highlight";
	case PDF_ANNOT_UNDERLINE: return "Underline";
	case PDF_ANNOT_SQUIGGLY: return "Squiggly";
	case PDF_ANNOT_STRIKE_OUT: return "StrikeOut";
	case PDF_ANNOT_REDACT: return "Redact";
	case PDF_ANNOT_STAMP: return "Stamp";
	case PDF_ANNOT_CARET: return "Caret";
	case PDF_ANNOT_INK: return "Ink";
	case PDF_ANNOT_POPUP: return "Popup";
	case PDF_ANNOT_FILE_ATTACHMENT: return "FileAttachment";
	case PDF_ANNOT_SOUND: return "Sound";
	case PDF_ANNOT_MOVIE: return "Movie";
	case PDF_ANNOT_RICH_MEDIA: return "RichMedia";
	case PDF_ANNOT_WIDGET: return "Widget";
	case PDF_ANNOT_SCREEN: return "Screen";
	case PDF_ANNOT_PRINTER_MARK: return "PrinterMark";
	case PDF_ANNOT_TRAP_NET: return "TrapNet";
	case PDF_ANNOT_WATERMARK: return "Watermark";
	case PDF_ANNOT_3D: return "3D";
	case PDF_ANNOT_PROJECTION: return "Projection";
	default: return "UNKNOWN";
	}
}

enum pdf_annot_type
pdf_annot_type_from_string(fz_context *ctx, const char *subtype)
{
	if (!strcmp("Text", subtype)) return PDF_ANNOT_TEXT;
	if (!strcmp("Link", subtype)) return PDF_ANNOT_LINK;
	if (!strcmp("FreeText", subtype)) return PDF_ANNOT_FREE_TEXT;
	if (!strcmp("Line", subtype)) return PDF_ANNOT_LINE;
	if (!strcmp("Square", subtype)) return PDF_ANNOT_SQUARE;
	if (!strcmp("Circle", subtype)) return PDF_ANNOT_CIRCLE;
	if (!strcmp("Polygon", subtype)) return PDF_ANNOT_POLYGON;
	if (!strcmp("PolyLine", subtype)) return PDF_ANNOT_POLY_LINE;
	if (!strcmp("Highlight", subtype)) return PDF_ANNOT_HIGHLIGHT;
	if (!strcmp("Underline", subtype)) return PDF_ANNOT_UNDERLINE;
	if (!strcmp("Squiggly", subtype)) return PDF_ANNOT_SQUIGGLY;
	if (!strcmp("StrikeOut", subtype)) return PDF_ANNOT_STRIKE_OUT;
	if (!strcmp("Redact", subtype)) return PDF_ANNOT_REDACT;
	if (!strcmp("Stamp", subtype)) return PDF_ANNOT_STAMP;
	if (!strcmp("Caret", subtype)) return PDF_ANNOT_CARET;
	if (!strcmp("Ink", subtype)) return PDF_ANNOT_INK;
	if (!strcmp("Popup", subtype)) return PDF_ANNOT_POPUP;
	if (!strcmp("FileAttachment", subtype)) return PDF_ANNOT_FILE_ATTACHMENT;
	if (!strcmp("Sound", subtype)) return PDF_ANNOT_SOUND;
	if (!strcmp("Movie", subtype)) return PDF_ANNOT_MOVIE;
	if (!strcmp("RichMedia", subtype)) return PDF_ANNOT_RICH_MEDIA;
	if (!strcmp("Widget", subtype)) return PDF_ANNOT_WIDGET;
	if (!strcmp("Screen", subtype)) return PDF_ANNOT_SCREEN;
	if (!strcmp("PrinterMark", subtype)) return PDF_ANNOT_PRINTER_MARK;
	if (!strcmp("TrapNet", subtype)) return PDF_ANNOT_TRAP_NET;
	if (!strcmp("Watermark", subtype)) return PDF_ANNOT_WATERMARK;
	if (!strcmp("3D", subtype)) return PDF_ANNOT_3D;
	if (!strcmp("Projection", subtype)) return PDF_ANNOT_PROJECTION;
	return PDF_ANNOT_UNKNOWN;
}

static void
begin_annot_op(fz_context *ctx, pdf_annot *annot, const char *op)
{
	pdf_begin_operation(ctx, annot->page->doc, op);
}

static void
end_annot_op(fz_context *ctx, pdf_annot *annot)
{
	pdf_end_operation(ctx, annot->page->doc);
}

static void
abandon_annot_op(fz_context *ctx, pdf_annot *annot)
{
	pdf_abandon_operation(ctx, annot->page->doc);
}

static int is_allowed_subtype(fz_context *ctx, pdf_annot *annot, pdf_obj *property, pdf_obj **allowed)
{
	pdf_obj *subtype;

	subtype = pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype));
	while (*allowed) {
		if (pdf_name_eq(ctx, subtype, *allowed))
			return 1;
		allowed++;
	}

	return 0;
}

static int is_allowed_subtype_wrap(fz_context *ctx, pdf_annot *annot, pdf_obj *property, pdf_obj **allowed)
{
	int ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
		ret = is_allowed_subtype(ctx, annot, property, allowed);
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

static void check_allowed_subtypes(fz_context *ctx, pdf_annot *annot, pdf_obj *property, pdf_obj **allowed)
{
	pdf_obj *subtype;

	subtype = pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype));
	if (!is_allowed_subtype(ctx, annot, property, allowed))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "%s annotations have no %s property", pdf_to_name(ctx, subtype), pdf_to_name(ctx, property));
}

pdf_annot *
pdf_create_annot_raw(fz_context *ctx, pdf_page *page, enum pdf_annot_type type)
{
	pdf_annot *annot = NULL;
	pdf_document *doc = page->doc;
	pdf_obj *annot_obj = pdf_new_dict(ctx, doc, 0);
	pdf_obj *ind_obj = NULL;

	fz_var(annot);
	fz_var(ind_obj);
	fz_try(ctx)
	{
		int ind_obj_num;
		const char *type_str;
		pdf_obj *annot_arr;

		type_str = pdf_string_from_annot_type(ctx, type);
		if (type == PDF_ANNOT_UNKNOWN)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot create unknown annotation");

		annot_arr = pdf_dict_get(ctx, page->obj, PDF_NAME(Annots));
		if (!pdf_is_array(ctx, annot_arr))
			annot_arr = pdf_dict_put_array(ctx, page->obj, PDF_NAME(Annots), 0);

		pdf_dict_put(ctx, annot_obj, PDF_NAME(Type), PDF_NAME(Annot));
		pdf_dict_put_name(ctx, annot_obj, PDF_NAME(Subtype), type_str);

		/*
			Both annotation object and annotation structure are now created.
			Insert the object in the hierarchy and the structure in the
			page's array.
		*/
		ind_obj_num = pdf_create_object(ctx, doc);
		pdf_update_object(ctx, doc, ind_obj_num, annot_obj);
		ind_obj = pdf_new_indirect(ctx, doc, ind_obj_num, 0);
		pdf_array_push(ctx, annot_arr, ind_obj);

		annot = pdf_new_annot(ctx, page, ind_obj);

		/*
			Linking must be done after any call that might throw because
			pdf_drop_annots below actually frees a list. Put the new annot
			at the end of the list, so that it will be drawn last.
		*/
		if (type == PDF_ANNOT_WIDGET)
		{
			*page->widget_tailp = annot;
			page->widget_tailp = &annot->next;
		}
		else
		{
			*page->annot_tailp = annot;
			page->annot_tailp = &annot->next;
		}
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, annot_obj);
		pdf_drop_obj(ctx, ind_obj);
	}
	fz_catch(ctx)
	{
		pdf_drop_annots(ctx, annot);
		fz_rethrow(ctx);
	}

	return pdf_keep_annot(ctx, annot);
}

fz_link *
pdf_create_link(fz_context *ctx, pdf_page *page, fz_rect bbox, const char *uri)
{
	fz_link *link = NULL;
	pdf_document *doc = page->doc;
	pdf_obj *annot_obj = pdf_new_dict(ctx, doc, 0);
	pdf_obj *ind_obj = NULL;
	pdf_obj *bs = NULL;
	pdf_obj *a = NULL;
	fz_link **linkp;
	fz_rect page_mediabox;
	fz_matrix page_ctm;
	fz_rect rect;

	fz_var(link);
	fz_var(ind_obj);
	fz_var(bs);
	fz_var(a);

	pdf_begin_operation(ctx, page->doc, "Create Link");

	fz_try(ctx)
	{
		int ind_obj_num;
		pdf_obj *annot_arr;

		pdf_page_transform(ctx, page, &page_mediabox, &page_ctm);
		page_ctm = fz_invert_matrix(page_ctm);
		rect = fz_transform_rect(bbox, page_ctm);

		annot_arr = pdf_dict_get(ctx, page->obj, PDF_NAME(Annots));
		if (!pdf_is_array(ctx, annot_arr))
			annot_arr = pdf_dict_put_array(ctx, page->obj, PDF_NAME(Annots), 0);

		pdf_dict_put(ctx, annot_obj, PDF_NAME(Type), PDF_NAME(Annot));
		pdf_dict_put(ctx, annot_obj, PDF_NAME(Subtype), PDF_NAME(Link));
		pdf_dict_put_rect(ctx, annot_obj, PDF_NAME(Rect), rect);

		bs = pdf_dict_put_array(ctx, annot_obj, PDF_NAME(BS), 4);
		pdf_dict_put(ctx, bs, PDF_NAME(S), PDF_NAME(S));
		pdf_dict_put(ctx, bs, PDF_NAME(Type), PDF_NAME(Border));
		pdf_dict_put_int(ctx, bs, PDF_NAME(W), 0);

		pdf_dict_put_drop(ctx, annot_obj, PDF_NAME(A),
			pdf_new_action_from_link(ctx, doc, uri));

		/*
			Both annotation object and annotation structure are now created.
			Insert the object in the hierarchy and the structure in the
			page's array.
		*/
		ind_obj_num = pdf_create_object(ctx, doc);
		pdf_update_object(ctx, doc, ind_obj_num, annot_obj);
		ind_obj = pdf_new_indirect(ctx, doc, ind_obj_num, 0);
		pdf_array_push(ctx, annot_arr, ind_obj);

		link = (fz_link *) pdf_new_link(ctx, page, bbox, uri, annot_obj);

		linkp = &page->links;

		while (*linkp != NULL)
			linkp = &(*linkp)->next;

		*linkp = link;
		pdf_end_operation(ctx, page->doc);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, annot_obj);
		pdf_drop_obj(ctx, ind_obj);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, page->doc);
		fz_rethrow(ctx);
	}

	return fz_keep_link(ctx, link);
}

void pdf_delete_link(fz_context *ctx, pdf_page *page, fz_link *link)
{
	fz_link **linkptr;
	pdf_obj *annots;
	int i;

	if (link == NULL || page == NULL || page != ((pdf_link *) link)->page)
		return;

	for (linkptr = &page->links; *linkptr; linkptr = &((*linkptr)->next))
	{
		if (*linkptr == link)
			break;
	}

	if (*linkptr == NULL)
		return;

	pdf_begin_operation(ctx, page->doc, "Delete Link");

	fz_try(ctx)
	{

		annots = pdf_dict_get(ctx, page->obj, PDF_NAME(Annots));
		i = pdf_array_find(ctx, annots, ((pdf_link *) link)->obj);
		if (i >= 0)
			pdf_array_delete(ctx, annots, i);
		*linkptr = link->next;
		link->next = NULL;
		fz_drop_link(ctx, link);
		pdf_end_operation(ctx, page->doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, page->doc);
		fz_rethrow(ctx);
	}
}

static pdf_obj *
pdf_add_popup_annot(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *annots, *popup;

	popup = pdf_dict_get(ctx, annot->obj, PDF_NAME(Popup));
	if (popup)
		return popup;

	annots = pdf_dict_get(ctx, annot->page->obj, PDF_NAME(Annots));
	if (!annots)
		return NULL;

	popup = pdf_add_new_dict(ctx, annot->page->doc, 4);
	pdf_array_push_drop(ctx, annots, popup);

	pdf_dict_put(ctx, popup, PDF_NAME(Type), PDF_NAME(Annot));
	pdf_dict_put(ctx, popup, PDF_NAME(Subtype), PDF_NAME(Popup));
	pdf_dict_put(ctx, popup, PDF_NAME(Parent), annot->obj);
	pdf_dict_put_rect(ctx, popup, PDF_NAME(Rect), fz_make_rect(0,0,0,0));

	pdf_dict_put(ctx, annot->obj, PDF_NAME(Popup), popup);

	return popup;
}

void pdf_set_annot_popup(fz_context *ctx, pdf_annot *annot, fz_rect rect)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *popup;
	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	inv_page_ctm = fz_invert_matrix(page_ctm);
	rect = fz_transform_rect(rect, inv_page_ctm);
	popup = pdf_add_popup_annot(ctx, annot);
	pdf_dict_put_rect(ctx, popup, PDF_NAME(Rect), rect);
}

fz_rect pdf_annot_popup(fz_context *ctx, pdf_annot *annot)
{
	fz_matrix page_ctm;
	fz_rect rect;
	pdf_obj *popup;
	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	popup = pdf_dict_get(ctx, annot->obj, PDF_NAME(Popup));
	rect = pdf_dict_get_rect(ctx, popup, PDF_NAME(Rect));
	return fz_transform_rect(rect, page_ctm);
}

pdf_annot *
pdf_create_annot(fz_context *ctx, pdf_page *page, enum pdf_annot_type type)
{
	static const float black[3] = { 0, 0, 0 };
	static const float red[3] = { 1, 0, 0 };
	static const float green[3] = { 0, 1, 0 };
	static const float blue[3] = { 0, 0, 1 };
	static const float yellow[3] = { 1, 1, 0 };
	static const float magenta[3] = { 1, 0, 1 };

	int flags = PDF_ANNOT_IS_PRINT; /* Make printable as default */

	pdf_annot *annot = NULL;

	fz_var(annot);

	pdf_begin_operation(ctx, page->doc, "Create Annotation");

	fz_try(ctx)
	{
		annot = pdf_create_annot_raw(ctx, page, type);

		switch (type)
		{
		default:
			break;

		case PDF_ANNOT_TEXT:
		case PDF_ANNOT_FILE_ATTACHMENT:
		case PDF_ANNOT_SOUND:
			{
				fz_rect icon_rect = { 12, 12, 12+20, 12+20 };
				flags = PDF_ANNOT_IS_PRINT | PDF_ANNOT_IS_NO_ZOOM | PDF_ANNOT_IS_NO_ROTATE;
				pdf_set_annot_rect(ctx, annot, icon_rect);
				pdf_set_annot_color(ctx, annot, 3, yellow);
				pdf_set_annot_popup(ctx, annot, fz_make_rect(32, 12, 32+200, 12+100));
			}
			break;

		case PDF_ANNOT_FREE_TEXT:
			{
				fz_rect text_rect = { 12, 12, 12+200, 12+100 };

				/* Use undocumented Adobe property to match page rotation. */
				int rot = pdf_dict_get_inheritable_int(ctx, page->obj, PDF_NAME(Rotate));
				if (rot != 0)
					pdf_dict_put_int(ctx, annot->obj, PDF_NAME(Rotate), rot);

				pdf_set_annot_rect(ctx, annot, text_rect);
				pdf_set_annot_border(ctx, annot, 0);
				pdf_set_annot_default_appearance(ctx, annot, "Helv", 12, nelem(black), black);
			}
			break;

		case PDF_ANNOT_STAMP:
			{
				fz_rect stamp_rect = { 12, 12, 12+190, 12+50 };
				pdf_set_annot_rect(ctx, annot, stamp_rect);
				pdf_set_annot_color(ctx, annot, 3, red);
				pdf_set_annot_icon_name(ctx, annot, "Draft");
			}
			break;

		case PDF_ANNOT_CARET:
			{
				fz_rect caret_rect = { 12, 12, 12+18, 12+15 };
				pdf_set_annot_rect(ctx, annot, caret_rect);
				pdf_set_annot_color(ctx, annot, 3, blue);
			}
			break;

		case PDF_ANNOT_LINE:
			{
				fz_point a = { 12, 12 }, b = { 12 + 100, 12 + 50 };
				pdf_set_annot_line(ctx, annot, a, b);
				pdf_set_annot_border(ctx, annot, 1);
				pdf_set_annot_color(ctx, annot, 3, red);
			}
			break;

		case PDF_ANNOT_SQUARE:
		case PDF_ANNOT_CIRCLE:
			{
				fz_rect shape_rect = { 12, 12, 12+100, 12+50 };
				pdf_set_annot_rect(ctx, annot, shape_rect);
				pdf_set_annot_border(ctx, annot, 1);
				pdf_set_annot_color(ctx, annot, 3, red);
			}
			break;

		case PDF_ANNOT_POLYGON:
		case PDF_ANNOT_POLY_LINE:
		case PDF_ANNOT_INK:
			pdf_set_annot_border(ctx, annot, 1);
			pdf_set_annot_color(ctx, annot, 3, red);
			break;

		case PDF_ANNOT_HIGHLIGHT:
			pdf_set_annot_color(ctx, annot, 3, yellow);
			break;
		case PDF_ANNOT_UNDERLINE:
			pdf_set_annot_color(ctx, annot, 3, green);
			break;
		case PDF_ANNOT_STRIKE_OUT:
			pdf_set_annot_color(ctx, annot, 3, red);
			break;
		case PDF_ANNOT_SQUIGGLY:
			pdf_set_annot_color(ctx, annot, 3, magenta);
			break;
		}

		pdf_dict_put(ctx, annot->obj, PDF_NAME(P), page->obj);
		pdf_dict_put_int(ctx, annot->obj, PDF_NAME(F), flags);
		pdf_end_operation(ctx, page->doc);
	}
	fz_catch(ctx)
	{
		pdf_drop_annot(ctx, annot);
		pdf_abandon_operation(ctx, page->doc);
		fz_rethrow(ctx);
	}

	return annot;
}

static int
remove_from_tree(fz_context *ctx, pdf_obj *arr, pdf_obj *item, pdf_cycle_list *cycle_up)
{
	pdf_cycle_list cycle;
	int i, n, res = 0;

	if (arr == NULL || pdf_cycle(ctx, &cycle, cycle_up, arr))
		return 0;

	n = pdf_array_len(ctx, arr);
	for (i = 0; i < n; ++i)
	{
		pdf_obj *obj = pdf_array_get(ctx, arr, i);
		if (obj == item)
		{
			pdf_array_delete(ctx, arr, i);
			res = 1;
			break;
		}

		if (remove_from_tree(ctx, pdf_dict_get(ctx, obj, PDF_NAME(Kids)), item, &cycle))
		{
			res = 1;
			break;
		}
	}

	return res;
}

void
pdf_delete_annot(fz_context *ctx, pdf_page *page, pdf_annot *annot)
{
	pdf_document *doc;
	pdf_annot **annotptr;
	pdf_obj *annot_arr, *popup;
	int i;
	int is_widget = 0;

	if (annot == NULL || page == NULL || page != annot->page)
		return;

	doc = page->doc;

	/* Look for the annot in the page's list */
	for (annotptr = &page->annots; *annotptr; annotptr = &(*annotptr)->next)
	{
		if (*annotptr == annot)
			break;
	}

	if (*annotptr == NULL)
	{
		is_widget = 1;

		/* Look also in the widget list*/
		for (annotptr = &page->widgets; *annotptr; annotptr = &(*annotptr)->next)
		{
			if (*annotptr == annot)
				break;
		}
	}

	/* Check the passed annotation was of this page */
	if (*annotptr == NULL)
		return;

	pdf_begin_operation(ctx, page->doc, "Delete Annotation");

	/* Remove annot from page's list */
	*annotptr = annot->next;

	/* If the removed annotation was the last in the list adjust the end pointer */
	if (*annotptr == NULL)
	{
		if (is_widget)
			page->widget_tailp = annotptr;
		else
			page->annot_tailp = annotptr;
	}

	fz_try(ctx)
	{
		/* Remove the annot from the "Annots" array. */
		annot_arr = pdf_dict_get(ctx, page->obj, PDF_NAME(Annots));
		i = pdf_array_find(ctx, annot_arr, annot->obj);
		if (i >= 0)
			pdf_array_delete(ctx, annot_arr, i);

		/* Remove the associated Popup annotation from the Annots array */
		popup = pdf_dict_get(ctx, annot->obj, PDF_NAME(Popup));
		if (popup)
		{
			i = pdf_array_find(ctx, annot_arr, popup);
			if (i >= 0)
				pdf_array_delete(ctx, annot_arr, i);
		}

		/* For a widget, remove also from the AcroForm tree */
		if (is_widget)
		{
			pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
			pdf_obj *acroform = pdf_dict_get(ctx, root, PDF_NAME(AcroForm));
			pdf_obj *fields = pdf_dict_get(ctx, acroform, PDF_NAME(Fields));
			(void)remove_from_tree(ctx, fields, annot->obj, NULL);
		}

		/* The garbage collection pass when saving will remove the annot object,
		 * removing it here may break files if multiple pages use the same annot. */
		pdf_end_operation(ctx, page->doc);
	}
	fz_always(ctx)
	{
		/* Drop the reference to annot previously held by the page list. */
		pdf_drop_annot(ctx, annot);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, page->doc);
		fz_rethrow(ctx);
	}
}

enum pdf_annot_type
pdf_annot_type(fz_context *ctx, pdf_annot *annot)
{
	enum pdf_annot_type ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		const char *subtype = pdf_dict_get_name(ctx, annot->obj, PDF_NAME(Subtype));
		ret = pdf_annot_type_from_string(ctx, subtype);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

int
pdf_annot_flags(fz_context *ctx, pdf_annot *annot)
{
	int ret;
	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
		ret = pdf_dict_get_int(ctx, annot->obj, PDF_NAME(F));
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

void
pdf_set_annot_flags(fz_context *ctx, pdf_annot *annot, int flags)
{
	begin_annot_op(ctx, annot, "Set flags");

	fz_try(ctx)
	{
		pdf_dict_put_int(ctx, annot->obj, PDF_NAME(F), flags);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

static pdf_obj *rect_subtypes[] = {
	PDF_NAME(Text),
	PDF_NAME(FreeText),
	PDF_NAME(Square),
	PDF_NAME(Circle),
	PDF_NAME(Redact),
	PDF_NAME(Stamp),
	PDF_NAME(Caret),
	PDF_NAME(Popup),
	PDF_NAME(FileAttachment),
	PDF_NAME(Sound),
	PDF_NAME(Movie),
	PDF_NAME(Widget),
	NULL,
};

int
pdf_annot_has_rect(fz_context *ctx, pdf_annot *annot)
{
	/* True for annotations where the user can manipulate the size or location
	 * of the annotation through the Rect.
	 * False for annotations where the Rect is computed from other
	 * annotation data such as InkList, QuadPoints, and Vertices.
	 */
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(Rect), rect_subtypes);
}

fz_rect
pdf_annot_rect(fz_context *ctx, pdf_annot *annot)
{
	fz_matrix page_ctm;
	fz_rect annot_rect;

	pdf_annot_push_local_xref(ctx, annot);
	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(Rect), rect_subtypes);
		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		annot_rect = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return fz_transform_rect(annot_rect, page_ctm);
}

void
pdf_set_annot_rect(fz_context *ctx, pdf_annot *annot, fz_rect rect)
{
	fz_matrix page_ctm, inv_page_ctm;

	pdf_begin_operation(ctx, annot->page->doc, "Set rectangle");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(Rect), rect_subtypes);

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		inv_page_ctm = fz_invert_matrix(page_ctm);
		rect = fz_transform_rect(rect, inv_page_ctm);

		pdf_dict_put_rect(ctx, annot->obj, PDF_NAME(Rect), rect);
		pdf_dirty_annot(ctx, annot);
		pdf_end_operation(ctx, annot->page->doc);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, annot->page->doc);
		fz_rethrow(ctx);
	}
}

const char *
pdf_annot_contents(fz_context *ctx, pdf_annot *annot)
{
	return pdf_dict_get_text_string(ctx, annot->obj, PDF_NAME(Contents));
}

void
pdf_set_annot_contents(fz_context *ctx, pdf_annot *annot, const char *text)
{
	begin_annot_op(ctx, annot, "Set contents");

	fz_try(ctx)
	{
		pdf_dict_put_text_string(ctx, annot->obj, PDF_NAME(Contents), text);
		pdf_dict_del(ctx, annot->obj, PDF_NAME(RC)); /* not supported */
		pdf_dirty_annot(ctx, annot);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}
}

int
pdf_annot_has_open(fz_context *ctx, pdf_annot *annot)
{
	int ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		pdf_obj *subtype = pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype));
		pdf_obj *popup = pdf_dict_get(ctx, annot->obj, PDF_NAME(Popup));
		ret = (subtype == PDF_NAME(Text) || popup);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

int
pdf_annot_is_open(fz_context *ctx, pdf_annot *annot)
{
	int ret = 0;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		pdf_obj *subtype = pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype));
		pdf_obj *popup = pdf_dict_get(ctx, annot->obj, PDF_NAME(Popup));
		if (popup)
			ret = pdf_dict_get_bool(ctx, popup, PDF_NAME(Open));
		else if (subtype == PDF_NAME(Text))
			ret = pdf_dict_get_bool(ctx, annot->obj, PDF_NAME(Open));
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

void
pdf_set_annot_is_open(fz_context *ctx, pdf_annot *annot, int is_open)
{
	begin_annot_op(ctx, annot, is_open ? "Open" : "Close");

	fz_try(ctx)
	{
		pdf_obj *subtype = pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype));
		pdf_obj *popup = pdf_dict_get(ctx, annot->obj, PDF_NAME(Popup));
		if (popup)
		{
			pdf_dict_put_bool(ctx, popup, PDF_NAME(Open), is_open);
			pdf_dirty_annot(ctx, annot);
		}
		else if (subtype == PDF_NAME(Text))
		{
			pdf_dict_put_bool(ctx, annot->obj, PDF_NAME(Open), is_open);
			pdf_dirty_annot(ctx, annot);
		}
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}
}

static pdf_obj *icon_name_subtypes[] = {
	PDF_NAME(FileAttachment),
	PDF_NAME(Sound),
	PDF_NAME(Stamp),
	PDF_NAME(Text),
	NULL,
};

int
pdf_annot_has_icon_name(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(Name), icon_name_subtypes);
}

const char *
pdf_annot_icon_name(fz_context *ctx, pdf_annot *annot)
{
	const char *ret;
	pdf_obj *name;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(Name), icon_name_subtypes);
		name = pdf_dict_get(ctx, annot->obj, PDF_NAME(Name));
		if (!name)
		{
			pdf_obj *subtype = pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype));
			if (pdf_name_eq(ctx, subtype, PDF_NAME(Text)))
			{
				ret = "Note";
				break;
			}
			if (pdf_name_eq(ctx, subtype, PDF_NAME(Stamp)))
			{
				ret = ""; // Should be "Draft" according to spec
				break;
			}
			if (pdf_name_eq(ctx, subtype, PDF_NAME(FileAttachment)))
			{
				ret = "PushPin";
				break;
			}
			if (pdf_name_eq(ctx, subtype, PDF_NAME(Sound)))
			{
				ret = "Speaker";
				break;
			}
		}
		ret = pdf_to_name(ctx, name);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

void
pdf_set_annot_icon_name(fz_context *ctx, pdf_annot *annot, const char *name)
{
	begin_annot_op(ctx, annot, "Set icon name");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(Name), icon_name_subtypes);
		if (name)
			pdf_dict_put_name(ctx, annot->obj, PDF_NAME(Name), name);
		else
			pdf_dict_del(ctx, annot->obj, PDF_NAME(Name));
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

int
pdf_annot_is_standard_stamp(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *name = pdf_dict_get(ctx, annot->obj, PDF_NAME(Name));
	if (pdf_name_eq(ctx, name, PDF_NAME(Approved))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(AsIs))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(Confidential))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(Departmental))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(Draft))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(Experimental))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(Expired))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(Final))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(ForComment))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(ForPublicRelease))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(NotApproved))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(NotForPublicRelease))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(Sold))) return 1;
	else if (pdf_name_eq(ctx, name, PDF_NAME(TopSecret))) return 1;
	else return 0;
}

enum pdf_line_ending pdf_line_ending_from_name(fz_context *ctx, pdf_obj *end)
{
	if (pdf_name_eq(ctx, end, PDF_NAME(None))) return PDF_ANNOT_LE_NONE;
	else if (pdf_name_eq(ctx, end, PDF_NAME(Square))) return PDF_ANNOT_LE_SQUARE;
	else if (pdf_name_eq(ctx, end, PDF_NAME(Circle))) return PDF_ANNOT_LE_CIRCLE;
	else if (pdf_name_eq(ctx, end, PDF_NAME(Diamond))) return PDF_ANNOT_LE_DIAMOND;
	else if (pdf_name_eq(ctx, end, PDF_NAME(OpenArrow))) return PDF_ANNOT_LE_OPEN_ARROW;
	else if (pdf_name_eq(ctx, end, PDF_NAME(ClosedArrow))) return PDF_ANNOT_LE_CLOSED_ARROW;
	else if (pdf_name_eq(ctx, end, PDF_NAME(Butt))) return PDF_ANNOT_LE_BUTT;
	else if (pdf_name_eq(ctx, end, PDF_NAME(ROpenArrow))) return PDF_ANNOT_LE_R_OPEN_ARROW;
	else if (pdf_name_eq(ctx, end, PDF_NAME(RClosedArrow))) return PDF_ANNOT_LE_R_CLOSED_ARROW;
	else if (pdf_name_eq(ctx, end, PDF_NAME(Slash))) return PDF_ANNOT_LE_SLASH;
	else return PDF_ANNOT_LE_NONE;
}

enum pdf_line_ending pdf_line_ending_from_string(fz_context *ctx, const char *end)
{
	if (!strcmp(end, "None")) return PDF_ANNOT_LE_NONE;
	else if (!strcmp(end, "Square")) return PDF_ANNOT_LE_SQUARE;
	else if (!strcmp(end, "Circle")) return PDF_ANNOT_LE_CIRCLE;
	else if (!strcmp(end, "Diamond")) return PDF_ANNOT_LE_DIAMOND;
	else if (!strcmp(end, "OpenArrow")) return PDF_ANNOT_LE_OPEN_ARROW;
	else if (!strcmp(end, "ClosedArrow")) return PDF_ANNOT_LE_CLOSED_ARROW;
	else if (!strcmp(end, "Butt")) return PDF_ANNOT_LE_BUTT;
	else if (!strcmp(end, "ROpenArrow")) return PDF_ANNOT_LE_R_OPEN_ARROW;
	else if (!strcmp(end, "RClosedArrow")) return PDF_ANNOT_LE_R_CLOSED_ARROW;
	else if (!strcmp(end, "Slash")) return PDF_ANNOT_LE_SLASH;
	else return PDF_ANNOT_LE_NONE;
}

pdf_obj *pdf_name_from_line_ending(fz_context *ctx, enum pdf_line_ending end)
{
	switch (end)
	{
	default:
	case PDF_ANNOT_LE_NONE: return PDF_NAME(None);
	case PDF_ANNOT_LE_SQUARE: return PDF_NAME(Square);
	case PDF_ANNOT_LE_CIRCLE: return PDF_NAME(Circle);
	case PDF_ANNOT_LE_DIAMOND: return PDF_NAME(Diamond);
	case PDF_ANNOT_LE_OPEN_ARROW: return PDF_NAME(OpenArrow);
	case PDF_ANNOT_LE_CLOSED_ARROW: return PDF_NAME(ClosedArrow);
	case PDF_ANNOT_LE_BUTT: return PDF_NAME(Butt);
	case PDF_ANNOT_LE_R_OPEN_ARROW: return PDF_NAME(ROpenArrow);
	case PDF_ANNOT_LE_R_CLOSED_ARROW: return PDF_NAME(RClosedArrow);
	case PDF_ANNOT_LE_SLASH: return PDF_NAME(Slash);
	}
}

const char *pdf_string_from_line_ending(fz_context *ctx, enum pdf_line_ending end)
{
	switch (end)
	{
	default:
	case PDF_ANNOT_LE_NONE: return "None";
	case PDF_ANNOT_LE_SQUARE: return "Square";
	case PDF_ANNOT_LE_CIRCLE: return "Circle";
	case PDF_ANNOT_LE_DIAMOND: return "Diamond";
	case PDF_ANNOT_LE_OPEN_ARROW: return "OpenArrow";
	case PDF_ANNOT_LE_CLOSED_ARROW: return "ClosedArrow";
	case PDF_ANNOT_LE_BUTT: return "Butt";
	case PDF_ANNOT_LE_R_OPEN_ARROW: return "ROpenArrow";
	case PDF_ANNOT_LE_R_CLOSED_ARROW: return "RClosedArrow";
	case PDF_ANNOT_LE_SLASH: return "Slash";
	}
}

static pdf_obj *line_ending_subtypes[] = {
	PDF_NAME(FreeText),
	PDF_NAME(Line),
	PDF_NAME(PolyLine),
	PDF_NAME(Polygon),
	NULL,
};

int
pdf_annot_has_line_ending_styles(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(LE), line_ending_subtypes);
}

void
pdf_annot_line_ending_styles(fz_context *ctx, pdf_annot *annot,
		enum pdf_line_ending *start_style,
		enum pdf_line_ending *end_style)
{
	pdf_obj *style;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(LE), line_ending_subtypes);

		style = pdf_dict_get(ctx, annot->obj, PDF_NAME(LE));
		*start_style = pdf_line_ending_from_name(ctx, pdf_array_get(ctx, style, 0));
		*end_style = pdf_line_ending_from_name(ctx, pdf_array_get(ctx, style, 1));
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

enum pdf_line_ending
pdf_annot_line_start_style(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *le = pdf_dict_get(ctx, annot->obj, PDF_NAME(LE));
	return pdf_line_ending_from_name(ctx, pdf_array_get(ctx, le, 0));
}

enum pdf_line_ending
pdf_annot_line_end_style(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *le = pdf_dict_get(ctx, annot->obj, PDF_NAME(LE));
	return pdf_line_ending_from_name(ctx, pdf_array_get(ctx, le, 1));
}

void
pdf_set_annot_line_ending_styles(fz_context *ctx, pdf_annot *annot,
		enum pdf_line_ending start_style,
		enum pdf_line_ending end_style)
{
	pdf_obj *style;

	begin_annot_op(ctx, annot, "Set line endings");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(LE), line_ending_subtypes);
		style = pdf_dict_put_array(ctx, annot->obj, PDF_NAME(LE), 2);
		pdf_array_put_drop(ctx, style, 0, pdf_name_from_line_ending(ctx, start_style));
		pdf_array_put_drop(ctx, style, 1, pdf_name_from_line_ending(ctx, end_style));
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void
pdf_set_annot_line_start_style(fz_context *ctx, pdf_annot *annot, enum pdf_line_ending s)
{
	enum pdf_line_ending e = pdf_annot_line_end_style(ctx, annot);
	pdf_set_annot_line_ending_styles(ctx, annot, s, e);
}

void
pdf_set_annot_line_end_style(fz_context *ctx, pdf_annot *annot, enum pdf_line_ending e)
{
	enum pdf_line_ending s = pdf_annot_line_start_style(ctx, annot);
	pdf_set_annot_line_ending_styles(ctx, annot, s, e);
}

float
pdf_annot_border(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *bs, *bs_w, *border;
	float ret = 1;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
		bs_w = pdf_dict_get(ctx, bs, PDF_NAME(W));
		if (pdf_is_number(ctx, bs_w))
		{
			ret = pdf_to_real(ctx, bs_w);
			break;
		}
		border = pdf_dict_get(ctx, annot->obj, PDF_NAME(Border));
		bs_w = pdf_array_get(ctx, border, 2);
		if (pdf_is_number(ctx, bs_w))
			ret = pdf_to_real(ctx, bs_w);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

void
pdf_set_annot_border(fz_context *ctx, pdf_annot *annot, float w)
{
	begin_annot_op(ctx, annot, "Set border");

	fz_try(ctx)
	{
		pdf_obj *bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
		if (!pdf_is_dict(ctx, bs))
			bs = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(BS), 1);
		pdf_dict_put(ctx, bs, PDF_NAME(Type), PDF_NAME(Border));
		pdf_dict_put_real(ctx, bs, PDF_NAME(W), w);

		pdf_dict_del(ctx, annot->obj, PDF_NAME(Border)); /* deprecated */
		pdf_dict_del(ctx, annot->obj, PDF_NAME(BE)); /* no effect */
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

static pdf_obj *border_style_subtypes[] = {
	PDF_NAME(Circle),
	PDF_NAME(FreeText),
	PDF_NAME(Ink),
	PDF_NAME(Line),
	PDF_NAME(Polygon),
	PDF_NAME(PolyLine),
	PDF_NAME(Square),
	NULL,
};

static pdf_obj *border_effect_subtypes[] = {
	PDF_NAME(Circle),
	PDF_NAME(FreeText),
	PDF_NAME(Polygon),
	PDF_NAME(Square),
	NULL,
};

int
pdf_annot_has_border(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(BS), border_style_subtypes);
}

int
pdf_annot_has_border_effect(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(BE), border_effect_subtypes);
}

enum pdf_border_style
pdf_annot_border_style(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *bs, *s;
	enum pdf_border_style style;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BS), border_style_subtypes);
		bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
		s = pdf_dict_get(ctx, bs, PDF_NAME(S));

		if (s == PDF_NAME(D))
			style = PDF_BORDER_STYLE_DASHED;
		else if (s == PDF_NAME(B))
			style = PDF_BORDER_STYLE_BEVELED;
		else if (s == PDF_NAME(I))
			style = PDF_BORDER_STYLE_INSET;
		else if (s == PDF_NAME(U))
			style = PDF_BORDER_STYLE_UNDERLINE;
		else
			style = PDF_BORDER_STYLE_SOLID;
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return style;
}

float
pdf_annot_border_width(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *border, *bs, *w;
	float width;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BS), border_style_subtypes);

		/* if values missing, fall back to deprecated /Border array */
		border = pdf_dict_get(ctx, annot->obj, PDF_NAME(Border));
		bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
		w = pdf_dict_get(ctx, bs, PDF_NAME(W));
		if (!pdf_is_number(ctx, w) && pdf_is_dict(ctx, border))
			w = pdf_array_get(ctx, border, 2);
		width = pdf_to_real(ctx, w);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return width;
}

int
pdf_annot_border_dash_count(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *bs, *d, *border;
	int count;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BS), border_style_subtypes);
		bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
		d = pdf_dict_get(ctx, bs, PDF_NAME(D));
		/* Query legacy dash pattern as a fallback */
		border = pdf_dict_get(ctx, annot->obj, PDF_NAME(Border));
		if (!pdf_is_array(ctx, d) && pdf_is_array(ctx, border))
			d = pdf_array_get(ctx, border, 3);
		count = pdf_array_len(ctx, d);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return count;
}

float
pdf_annot_border_dash_item(fz_context *ctx, pdf_annot *annot, int i)
{
	pdf_obj *bs, *d, *border;
	float length;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BS), border_style_subtypes);
		bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
		d = pdf_dict_get(ctx, bs, PDF_NAME(D));
		/* Query legacy dash pattern as a fallback */
		border = pdf_dict_get(ctx, annot->obj, PDF_NAME(Border));
		if (!pdf_is_array(ctx, d) && pdf_is_array(ctx, border))
			d = pdf_array_get(ctx, border, 3);
		length = pdf_array_get_real(ctx, d, i);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return length;
}

enum pdf_border_effect
pdf_annot_border_effect(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *be;
	enum pdf_border_effect effect;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BE), border_effect_subtypes);
		be = pdf_dict_get(ctx, annot->obj, PDF_NAME(BE));
		if (pdf_dict_get(ctx, be, PDF_NAME(S)) == PDF_NAME(C))
			effect = PDF_BORDER_EFFECT_CLOUDY;
		else
			effect = PDF_BORDER_EFFECT_NONE;
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return effect;
}

float
pdf_annot_border_effect_intensity(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *be;
	float intensity;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BE), border_effect_subtypes);
		be = pdf_dict_get(ctx, annot->obj, PDF_NAME(BE));
		intensity = pdf_dict_get_real(ctx, be, PDF_NAME(I));
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return intensity;
}

void
pdf_set_annot_border_width(fz_context *ctx, pdf_annot *annot, float width)
{
	pdf_obj *bs;

	begin_annot_op(ctx, annot, "Set border width");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BS), border_style_subtypes);
		bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
		if (!pdf_is_dict(ctx, bs))
			bs = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(BS), 1);
		pdf_dict_put(ctx, bs, PDF_NAME(Type), PDF_NAME(Border));
		pdf_dict_put_real(ctx, bs, PDF_NAME(W), width);
		pdf_dict_del(ctx, annot->obj, PDF_NAME(Border)); /* deprecated */
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void
pdf_set_annot_border_style(fz_context *ctx, pdf_annot *annot, enum pdf_border_style style)
{
	pdf_obj *bs, *s;

	begin_annot_op(ctx, annot, "Set border style");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BS), border_style_subtypes);
		bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
		if (!pdf_is_dict(ctx, bs))
			bs = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(BS), 1);
		pdf_dict_put(ctx, bs, PDF_NAME(Type), PDF_NAME(Border));
		switch (style)
		{
		default:
		case PDF_BORDER_STYLE_SOLID: s = PDF_NAME(S); break;
		case PDF_BORDER_STYLE_DASHED: s = PDF_NAME(D); break;
		case PDF_BORDER_STYLE_BEVELED: s = PDF_NAME(B); break;
		case PDF_BORDER_STYLE_INSET: s = PDF_NAME(I); break;
		case PDF_BORDER_STYLE_UNDERLINE: s = PDF_NAME(U); break;
		}
		pdf_dict_put(ctx, bs, PDF_NAME(S), s);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void
pdf_clear_annot_border_dash(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *bs, *border;

	begin_annot_op(ctx, annot, "Clear border dash pattern");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BS), border_style_subtypes);
		bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
		if (!pdf_is_dict(ctx, bs))
			bs = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(BS), 1);
		pdf_dict_del(ctx, bs, PDF_NAME(D));
		/* Remove legacy dash pattern */
		border = pdf_dict_get(ctx, annot->obj, PDF_NAME(Border));
		if (pdf_is_array(ctx, border))
			pdf_array_delete(ctx, border, 3);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void
pdf_add_annot_border_dash_item(fz_context *ctx, pdf_annot *annot, float length)
{
	pdf_obj *bs, *d, *border;

	begin_annot_op(ctx, annot, "Add border dash pattern item");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BS), border_style_subtypes);
		bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
		if (!pdf_is_dict(ctx, bs))
			bs = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(BS), 1);
		d = pdf_dict_get(ctx, bs, PDF_NAME(D));
		if (!pdf_is_array(ctx, d))
			d = pdf_dict_put_array(ctx, bs, PDF_NAME(D), 1);
		pdf_array_push_real(ctx, d, length);
		/* Remove legacy dash pattern */
		border = pdf_dict_get(ctx, annot->obj, PDF_NAME(Border));
		if (pdf_is_array(ctx, border))
			pdf_array_delete(ctx, border, 3);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void
pdf_set_annot_border_effect(fz_context *ctx, pdf_annot *annot, enum pdf_border_effect effect)
{
	pdf_obj *be, *s;

	begin_annot_op(ctx, annot, "Set border effect");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BE), border_effect_subtypes);
		be = pdf_dict_get(ctx, annot->obj, PDF_NAME(BE));
		if (!pdf_is_dict(ctx, be))
			be = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(BE), 1);
		switch (effect)
		{
		default:
		case PDF_BORDER_EFFECT_NONE: s = PDF_NAME(S); break;
		case PDF_BORDER_EFFECT_CLOUDY: s = PDF_NAME(C); break;
		}
		pdf_dict_put(ctx, be, PDF_NAME(S), s);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void
pdf_set_annot_border_effect_intensity(fz_context *ctx, pdf_annot *annot, float intensity)
{
	pdf_obj *be;

	begin_annot_op(ctx, annot, "Set border effect intensity");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(BE), border_effect_subtypes);
		be = pdf_dict_get(ctx, annot->obj, PDF_NAME(BE));
		if (!pdf_is_dict(ctx, be))
			be = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(BE), 1);
		pdf_dict_put_real(ctx, be, PDF_NAME(I), intensity);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

fz_text_language
pdf_document_language(fz_context *ctx, pdf_document *doc)
{
	pdf_obj *trailer = pdf_trailer(ctx, doc);
	pdf_obj *root = pdf_dict_get(ctx, trailer, PDF_NAME(Root));
	pdf_obj *lang = pdf_dict_get(ctx, root, PDF_NAME(Lang));
	return fz_text_language_from_string(pdf_to_text_string(ctx, lang));
}

void pdf_set_document_language(fz_context *ctx, pdf_document *doc, fz_text_language lang)
{
	pdf_obj *trailer = pdf_trailer(ctx, doc);
	pdf_obj *root = pdf_dict_get(ctx, trailer, PDF_NAME(Root));
	char buf[8];
	if (lang == FZ_LANG_UNSET)
		pdf_dict_del(ctx, root, PDF_NAME(Lang));
	else
		pdf_dict_put_text_string(ctx, root, PDF_NAME(Lang), fz_string_from_text_language(buf, lang));
}

fz_text_language
pdf_annot_language(fz_context *ctx, pdf_annot *annot)
{
	fz_text_language ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		pdf_obj *lang = pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(Lang));
		if (lang)
			ret = fz_text_language_from_string(pdf_to_str_buf(ctx, lang));
		else
			ret = pdf_document_language(ctx, annot->page->doc);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

void
pdf_set_annot_language(fz_context *ctx, pdf_annot *annot, fz_text_language lang)
{
	char buf[8];

	begin_annot_op(ctx, annot, "Set language");

	fz_try(ctx)
	{
		if (lang == FZ_LANG_UNSET)
			pdf_dict_del(ctx, annot->obj, PDF_NAME(Lang));
		else
			pdf_dict_put_text_string(ctx, annot->obj, PDF_NAME(Lang), fz_string_from_text_language(buf, lang));
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

int
pdf_annot_quadding(fz_context *ctx, pdf_annot *annot)
{
	int q = pdf_dict_get_int(ctx, annot->obj, PDF_NAME(Q));
	return (q < 0 || q > 2) ? 0 : q;
}

void
pdf_set_annot_quadding(fz_context *ctx, pdf_annot *annot, int q)
{
	q = (q < 0 || q > 2) ? 0 : q;

	begin_annot_op(ctx, annot, "Set quadding");

	fz_try(ctx)
	{
		pdf_dict_put_int(ctx, annot->obj, PDF_NAME(Q), q);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

float pdf_annot_opacity(fz_context *ctx, pdf_annot *annot)
{
	float ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
		ret = pdf_dict_get_real_default(ctx, annot->obj, PDF_NAME(CA), 1);
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

void pdf_set_annot_opacity(fz_context *ctx, pdf_annot *annot, float opacity)
{
	begin_annot_op(ctx, annot, "Set opacity");

	fz_try(ctx)
	{
		if (opacity != 1)
			pdf_dict_put_real(ctx, annot->obj, PDF_NAME(CA), opacity);
		else
			pdf_dict_del(ctx, annot->obj, PDF_NAME(CA));
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

static void pdf_annot_color_imp(fz_context *ctx, pdf_obj *arr, int *n, float color[4])
{
	switch (pdf_array_len(ctx, arr))
	{
	case 0:
		if (n)
			*n = 0;
		break;
	case 1:
	case 2:
		if (n)
			*n = 1;
		if (color)
			color[0] = pdf_array_get_real(ctx, arr, 0);
		break;
	case 3:
		if (n)
			*n = 3;
		if (color)
		{
			color[0] = pdf_array_get_real(ctx, arr, 0);
			color[1] = pdf_array_get_real(ctx, arr, 1);
			color[2] = pdf_array_get_real(ctx, arr, 2);
		}
		break;
	case 4:
	default:
		if (n)
			*n = 4;
		if (color)
		{
			color[0] = pdf_array_get_real(ctx, arr, 0);
			color[1] = pdf_array_get_real(ctx, arr, 1);
			color[2] = pdf_array_get_real(ctx, arr, 2);
			color[3] = pdf_array_get_real(ctx, arr, 3);
		}
		break;
	}
}

static int pdf_annot_color_rgb(fz_context *ctx, pdf_obj *arr, float rgb[3])
{
	float color[4];
	int n;
	pdf_annot_color_imp(ctx, arr, &n, color);
	if (n == 0)
	{
		return 0;
	}
	else if (n == 1)
	{
		rgb[0] = rgb[1] = rgb[2] = color[0];
	}
	else if (n == 3)
	{
		rgb[0] = color[0];
		rgb[1] = color[1];
		rgb[2] = color[2];
	}
	else if (n == 4)
	{
		rgb[0] = 1 - fz_min(1, color[0] + color[3]);
		rgb[1] = 1 - fz_min(1, color[1] + color[3]);
		rgb[2] = 1 - fz_min(1, color[2] + color[3]);
	}
	return 1;
}

static void pdf_set_annot_color_imp(fz_context *ctx, pdf_annot *annot, pdf_obj *key, int n, const float *color, pdf_obj **allowed)
{
	pdf_obj *arr;

	if (allowed)
		check_allowed_subtypes(ctx, annot, key, allowed);
	if (n != 0 && n != 1 && n != 3 && n != 4)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "color must be 0, 1, 3 or 4 components");
	if (!color)
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "no color given");

	arr = pdf_dict_put_array(ctx, annot->obj, key, n);
	fz_try(ctx)
	{
		switch (n)
		{
		case 1:
			pdf_array_push_real(ctx, arr, color[0]);
			break;
		case 3:
			pdf_array_push_real(ctx, arr, color[0]);
			pdf_array_push_real(ctx, arr, color[1]);
			pdf_array_push_real(ctx, arr, color[2]);
			break;
		case 4:
			pdf_array_push_real(ctx, arr, color[0]);
			pdf_array_push_real(ctx, arr, color[1]);
			pdf_array_push_real(ctx, arr, color[2]);
			pdf_array_push_real(ctx, arr, color[3]);
			break;
		}
	}
	fz_catch(ctx)
		fz_rethrow(ctx);

	pdf_dirty_annot(ctx, annot);
}

void
pdf_annot_color(fz_context *ctx, pdf_annot *annot, int *n, float color[4])
{
	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		pdf_obj *c = pdf_dict_get(ctx, annot->obj, PDF_NAME(C));
		pdf_annot_color_imp(ctx, c, n, color);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_annot_MK_BG(fz_context *ctx, pdf_annot *annot, int *n, float color[4])
{
	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		pdf_obj *mk_bg = pdf_dict_get(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(BG));
		pdf_annot_color_imp(ctx, mk_bg, n, color);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

int
pdf_annot_MK_BG_rgb(fz_context *ctx, pdf_annot *annot, float rgb[3])
{
	int ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		pdf_obj *mk_bg = pdf_dict_get(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(BG));
		ret = pdf_annot_color_rgb(ctx, mk_bg, rgb);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

void
pdf_annot_MK_BC(fz_context *ctx, pdf_annot *annot, int *n, float color[4])
{
	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		pdf_obj *mk_bc = pdf_dict_get(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(BC));
		pdf_annot_color_imp(ctx, mk_bc, n, color);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

int
pdf_annot_MK_BC_rgb(fz_context *ctx, pdf_annot *annot, float rgb[3])
{
	int ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		pdf_obj *mk_bc = pdf_dict_get(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(BC));
		ret = pdf_annot_color_rgb(ctx, mk_bc, rgb);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

void
pdf_set_annot_color(fz_context *ctx, pdf_annot *annot, int n, const float *color)
{
	begin_annot_op(ctx, annot, "Set color");

	fz_try(ctx)
	{
		pdf_set_annot_color_imp(ctx, annot, PDF_NAME(C), n, color, NULL);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}
}

static pdf_obj *interior_color_subtypes[] = {
	PDF_NAME(Circle),
	PDF_NAME(Line),
	PDF_NAME(PolyLine),
	PDF_NAME(Polygon),
	PDF_NAME(Square),
	NULL,
};

int
pdf_annot_has_interior_color(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(IC), interior_color_subtypes);
}

void
pdf_annot_interior_color(fz_context *ctx, pdf_annot *annot, int *n, float color[4])
{
	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		pdf_obj *ic = pdf_dict_get(ctx, annot->obj, PDF_NAME(IC));
		pdf_annot_color_imp(ctx, ic, n, color);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_set_annot_interior_color(fz_context *ctx, pdf_annot *annot, int n, const float *color)
{
	begin_annot_op(ctx, annot, "Set interior color");

	fz_try(ctx)
	{
		pdf_set_annot_color_imp(ctx, annot, PDF_NAME(IC), n, color, interior_color_subtypes);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}
}

static pdf_obj *line_subtypes[] = {
	PDF_NAME(Line),
	NULL,
};

int
pdf_annot_has_line(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(L), line_subtypes);
}

void
pdf_annot_line(fz_context *ctx, pdf_annot *annot, fz_point *a, fz_point *b)
{
	fz_matrix page_ctm;
	pdf_obj *line;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(L), line_subtypes);

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);

		line = pdf_dict_get(ctx, annot->obj, PDF_NAME(L));
		a->x = pdf_array_get_real(ctx, line, 0);
		a->y = pdf_array_get_real(ctx, line, 1);
		b->x = pdf_array_get_real(ctx, line, 2);
		b->y = pdf_array_get_real(ctx, line, 3);
		*a = fz_transform_point(*a, page_ctm);
		*b = fz_transform_point(*b, page_ctm);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void
pdf_set_annot_line(fz_context *ctx, pdf_annot *annot, fz_point a, fz_point b)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *line;

	begin_annot_op(ctx, annot, "Set line");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(L), line_subtypes);

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		inv_page_ctm = fz_invert_matrix(page_ctm);

		a = fz_transform_point(a, inv_page_ctm);
		b = fz_transform_point(b, inv_page_ctm);

		line = pdf_dict_put_array(ctx, annot->obj, PDF_NAME(L), 4);
		pdf_array_push_real(ctx, line, a.x);
		pdf_array_push_real(ctx, line, a.y);
		pdf_array_push_real(ctx, line, b.x);
		pdf_array_push_real(ctx, line, b.y);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

static pdf_obj *vertices_subtypes[] = {
	PDF_NAME(PolyLine),
	PDF_NAME(Polygon),
	NULL,
};

int
pdf_annot_has_vertices(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);
}

int
pdf_annot_vertex_count(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *vertices;
	int ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);
		vertices = pdf_dict_get(ctx, annot->obj, PDF_NAME(Vertices));
		ret = pdf_array_len(ctx, vertices) / 2;
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

fz_point
pdf_annot_vertex(fz_context *ctx, pdf_annot *annot, int i)
{
	pdf_obj *vertices;
	fz_matrix page_ctm;
	fz_point point;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);

		vertices = pdf_dict_get(ctx, annot->obj, PDF_NAME(Vertices));

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);

		point.x = pdf_array_get_real(ctx, vertices, i * 2);
		point.y = pdf_array_get_real(ctx, vertices, i * 2 + 1);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return fz_transform_point(point, page_ctm);
}

void
pdf_set_annot_vertices(fz_context *ctx, pdf_annot *annot, int n, const fz_point *v)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *vertices;
	fz_point point;
	int i;

	begin_annot_op(ctx, annot, "Set points");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);
		if (n <= 0 || !v)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid number of vertices");

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		inv_page_ctm = fz_invert_matrix(page_ctm);

		vertices = pdf_dict_put_array(ctx, annot->obj, PDF_NAME(Vertices), n * 2);
		for (i = 0; i < n; ++i)
		{
			point = fz_transform_point(v[i], inv_page_ctm);
			pdf_array_push_real(ctx, vertices, point.x);
			pdf_array_push_real(ctx, vertices, point.y);
		}
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void pdf_clear_annot_vertices(fz_context *ctx, pdf_annot *annot)
{
	begin_annot_op(ctx, annot, "Clear vertices");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);
		pdf_dict_del(ctx, annot->obj, PDF_NAME(Vertices));
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void pdf_add_annot_vertex(fz_context *ctx, pdf_annot *annot, fz_point p)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *vertices;

	begin_annot_op(ctx, annot, "Add point");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		inv_page_ctm = fz_invert_matrix(page_ctm);

		vertices = pdf_dict_get(ctx, annot->obj, PDF_NAME(Vertices));
		if (!pdf_is_array(ctx, vertices))
			vertices = pdf_dict_put_array(ctx, annot->obj, PDF_NAME(Vertices), 32);

		p = fz_transform_point(p, inv_page_ctm);
		pdf_array_push_real(ctx, vertices, p.x);
		pdf_array_push_real(ctx, vertices, p.y);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void pdf_set_annot_vertex(fz_context *ctx, pdf_annot *annot, int i, fz_point p)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *vertices;

	begin_annot_op(ctx, annot, "Set point");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		inv_page_ctm = fz_invert_matrix(page_ctm);

		p = fz_transform_point(p, inv_page_ctm);

		vertices = pdf_dict_get(ctx, annot->obj, PDF_NAME(Vertices));
		pdf_array_put_real(ctx, vertices, i * 2 + 0, p.x);
		pdf_array_put_real(ctx, vertices, i * 2 + 1, p.x);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}
}

static pdf_obj *quad_point_subtypes[] = {
	PDF_NAME(Highlight),
	PDF_NAME(Link),
	PDF_NAME(Squiggly),
	PDF_NAME(StrikeOut),
	PDF_NAME(Underline),
	PDF_NAME(Redact),
	NULL,
};

int
pdf_annot_has_quad_points(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);
}

int
pdf_annot_quad_point_count(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *quad_points;
	int ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);
		quad_points = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
		ret = pdf_array_len(ctx, quad_points) / 8;
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

fz_quad
pdf_annot_quad_point(fz_context *ctx, pdf_annot *annot, int idx)
{
	pdf_obj *quad_points;
	fz_matrix page_ctm;
	float v[8];
	int i;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);
		quad_points = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);

		for (i = 0; i < 8; i += 2)
		{
			fz_point point;
			point.x = pdf_array_get_real(ctx, quad_points, idx * 8 + i + 0);
			point.y = pdf_array_get_real(ctx, quad_points, idx * 8 + i + 1);
			point = fz_transform_point(point, page_ctm);
			v[i+0] = point.x;
			v[i+1] = point.y;
		}
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return fz_make_quad(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
}

void
pdf_set_annot_quad_points(fz_context *ctx, pdf_annot *annot, int n, const fz_quad *q)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *quad_points;
	fz_quad quad;
	int i;

	begin_annot_op(ctx, annot, "Set quad points");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);
		if (n <= 0 || !q)
			fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid number of quadrilaterals");

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		inv_page_ctm = fz_invert_matrix(page_ctm);

		quad_points = pdf_dict_put_array(ctx, annot->obj, PDF_NAME(QuadPoints), n);
		for (i = 0; i < n; ++i)
		{
			quad = fz_transform_quad(q[i], inv_page_ctm);
			pdf_array_push_real(ctx, quad_points, quad.ul.x);
			pdf_array_push_real(ctx, quad_points, quad.ul.y);
			pdf_array_push_real(ctx, quad_points, quad.ur.x);
			pdf_array_push_real(ctx, quad_points, quad.ur.y);
			pdf_array_push_real(ctx, quad_points, quad.ll.x);
			pdf_array_push_real(ctx, quad_points, quad.ll.y);
			pdf_array_push_real(ctx, quad_points, quad.lr.x);
			pdf_array_push_real(ctx, quad_points, quad.lr.y);
		}

		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void
pdf_clear_annot_quad_points(fz_context *ctx, pdf_annot *annot)
{
	begin_annot_op(ctx, annot, "Clear quad points");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);
		pdf_dict_del(ctx, annot->obj, PDF_NAME(QuadPoints));
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void
pdf_add_annot_quad_point(fz_context *ctx, pdf_annot *annot, fz_quad quad)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *quad_points;

	begin_annot_op(ctx, annot, "Add quad point");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		inv_page_ctm = fz_invert_matrix(page_ctm);

		quad_points = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
		if (!pdf_is_array(ctx, quad_points))
			quad_points = pdf_dict_put_array(ctx, annot->obj, PDF_NAME(QuadPoints), 8);

		/* Contrary to the specification, the points within a QuadPoint are NOT ordered
		 * in a counterclockwise fashion. Experiments with Adobe's implementation
		 * indicates a cross-wise ordering is intended: ul, ur, ll, lr.
		 */
		quad = fz_transform_quad(quad, inv_page_ctm);
		pdf_array_push_real(ctx, quad_points, quad.ul.x);
		pdf_array_push_real(ctx, quad_points, quad.ul.y);
		pdf_array_push_real(ctx, quad_points, quad.ur.x);
		pdf_array_push_real(ctx, quad_points, quad.ur.y);
		pdf_array_push_real(ctx, quad_points, quad.ll.x);
		pdf_array_push_real(ctx, quad_points, quad.ll.y);
		pdf_array_push_real(ctx, quad_points, quad.lr.x);
		pdf_array_push_real(ctx, quad_points, quad.lr.y);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

static pdf_obj *ink_list_subtypes[] = {
	PDF_NAME(Ink),
	NULL,
};

int
pdf_annot_has_ink_list(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);
}

int
pdf_annot_ink_list_count(fz_context *ctx, pdf_annot *annot)
{
	int ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		pdf_obj *ink_list;
		check_allowed_subtypes(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);
		ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
		ret = pdf_array_len(ctx, ink_list);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

int
pdf_annot_ink_list_stroke_count(fz_context *ctx, pdf_annot *annot, int i)
{
	pdf_obj *ink_list;
	pdf_obj *stroke;
	int ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);
		ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
		stroke = pdf_array_get(ctx, ink_list, i);
		ret = pdf_array_len(ctx, stroke) / 2;
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

fz_point
pdf_annot_ink_list_stroke_vertex(fz_context *ctx, pdf_annot *annot, int i, int k)
{
	pdf_obj *ink_list;
	pdf_obj *stroke;
	fz_matrix page_ctm;
	fz_point point;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);

		ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
		stroke = pdf_array_get(ctx, ink_list, i);

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);

		point.x = pdf_array_get_real(ctx, stroke, k * 2 + 0);
		point.y = pdf_array_get_real(ctx, stroke, k * 2 + 1);
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return fz_transform_point(point, page_ctm);
}

/* FIXME: try/catch required for memory exhaustion */
void
pdf_set_annot_ink_list(fz_context *ctx, pdf_annot *annot, int n, const int *count, const fz_point *v)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *ink_list = NULL, *stroke;
	fz_point point;
	int i, k;

	fz_var(ink_list);

	begin_annot_op(ctx, annot, "Set ink list");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		inv_page_ctm = fz_invert_matrix(page_ctm);

		ink_list = pdf_dict_put_array(ctx, annot->obj, PDF_NAME(InkList), n);
		for (i = 0; i < n; ++i)
		{
			stroke = pdf_array_push_array(ctx, ink_list, count[i] * 2);

			/* Although we have dropped our reference to stroke,
			 * it's still valid because we ink_list holds one, and
			 * we hold a reference to that. */
			for (k = 0; k < count[i]; ++k)
			{
				point = fz_transform_point(*v++, inv_page_ctm);
				pdf_array_push_real(ctx, stroke, point.x);
				pdf_array_push_real(ctx, stroke, point.y);
			}
		}
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void
pdf_clear_annot_ink_list(fz_context *ctx, pdf_annot *annot)
{
	begin_annot_op(ctx, annot, "Clear ink list");

	fz_try(ctx)
	{
		pdf_dict_del(ctx, annot->obj, PDF_NAME(InkList));
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void pdf_add_annot_ink_list_stroke(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *ink_list;

	begin_annot_op(ctx, annot, "Add ink list stroke");

	fz_try(ctx)
	{
		ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
		if (!pdf_is_array(ctx, ink_list))
			ink_list = pdf_dict_put_array(ctx, annot->obj, PDF_NAME(InkList), 10);

		pdf_array_push_array(ctx, ink_list, 16);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void pdf_add_annot_ink_list_stroke_vertex(fz_context *ctx, pdf_annot *annot, fz_point p)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *ink_list, *stroke;

	begin_annot_op(ctx, annot, "Add ink list stroke point");

	fz_try(ctx)
	{
		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		inv_page_ctm = fz_invert_matrix(page_ctm);

		ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
		stroke = pdf_array_get(ctx, ink_list, pdf_array_len(ctx, ink_list)-1);

		p = fz_transform_point(p, inv_page_ctm);
		pdf_array_push_real(ctx, stroke, p.x);
		pdf_array_push_real(ctx, stroke, p.y);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

void
pdf_add_annot_ink_list(fz_context *ctx, pdf_annot *annot, int n, fz_point p[])
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *ink_list, *stroke;
	int i;

	begin_annot_op(ctx, annot, "Add ink list");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);

		pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
		inv_page_ctm = fz_invert_matrix(page_ctm);

		ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
		if (!pdf_is_array(ctx, ink_list))
			ink_list = pdf_dict_put_array(ctx, annot->obj, PDF_NAME(InkList), 10);

		stroke = pdf_array_push_array(ctx, ink_list, n * 2);
		for (i = 0; i < n; ++i)
		{
			fz_point tp = fz_transform_point(p[i], inv_page_ctm);
			pdf_array_push_real(ctx, stroke, tp.x);
			pdf_array_push_real(ctx, stroke, tp.y);
		}
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

static pdf_obj *markup_subtypes[] = {
	PDF_NAME(Text),
	PDF_NAME(FreeText),
	PDF_NAME(Line),
	PDF_NAME(Square),
	PDF_NAME(Circle),
	PDF_NAME(Polygon),
	PDF_NAME(PolyLine),
	PDF_NAME(Highlight),
	PDF_NAME(Underline),
	PDF_NAME(Squiggly),
	PDF_NAME(StrikeOut),
	PDF_NAME(Redact),
	PDF_NAME(Stamp),
	PDF_NAME(Caret),
	PDF_NAME(Ink),
	PDF_NAME(FileAttachment),
	PDF_NAME(Sound),
	NULL,
};

/*
	Get annotation's modification date in seconds since the epoch.
*/
int64_t
pdf_annot_modification_date(fz_context *ctx, pdf_annot *annot)
{
	int64_t ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		ret = pdf_dict_get_date(ctx, annot->obj, PDF_NAME(M));
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

/*
	Get annotation's creation date in seconds since the epoch.
*/
int64_t
pdf_annot_creation_date(fz_context *ctx, pdf_annot *annot)
{
	int64_t ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
		ret = pdf_dict_get_date(ctx, annot->obj, PDF_NAME(CreationDate));
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

/*
	Set annotation's modification date in seconds since the epoch.
*/
void
pdf_set_annot_modification_date(fz_context *ctx, pdf_annot *annot, int64_t secs)
{
	begin_annot_op(ctx, annot, "Set modification date");

	fz_try(ctx)
	{
		pdf_dict_put_date(ctx, annot->obj, PDF_NAME(M), secs);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

/*
	Set annotation's creation date in seconds since the epoch.
*/
void
pdf_set_annot_creation_date(fz_context *ctx, pdf_annot *annot, int64_t secs)
{
	begin_annot_op(ctx, annot, "Set creation date");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(CreationDate), markup_subtypes);
		pdf_dict_put_date(ctx, annot->obj, PDF_NAME(CreationDate), secs);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

int
pdf_annot_has_author(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(T), markup_subtypes);
}

const char *
pdf_annot_author(fz_context *ctx, pdf_annot *annot)
{
	const char *ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(T), markup_subtypes);
		ret = pdf_dict_get_text_string(ctx, annot->obj, PDF_NAME(T));
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

void
pdf_set_annot_author(fz_context *ctx, pdf_annot *annot, const char *author)
{
	begin_annot_op(ctx, annot, "Set author");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(T), markup_subtypes);
		pdf_dict_put_text_string(ctx, annot->obj, PDF_NAME(T), author);
		pdf_dirty_annot(ctx, annot);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}
}

void
pdf_parse_default_appearance(fz_context *ctx, const char *da, const char **font, float *size, int *n, float color[4])
{
	char buf[100], *p = buf, *tok, *end;
	float stack[4] = { 0, 0, 0, 0 };
	int top = 0;

	*font = "Helv";
	*size = 12;
	*n = 0;
	color[0] = color[1] = color[2] = color[3] = 0;

	fz_strlcpy(buf, da, sizeof buf);
	while ((tok = fz_strsep(&p, " \n\r\t")) != NULL)
	{
		if (tok[0] == 0)
			;
		else if (tok[0] == '/')
		{
			if (!strcmp(tok+1, "Cour")) *font = "Cour";
			if (!strcmp(tok+1, "Helv")) *font = "Helv";
			if (!strcmp(tok+1, "TiRo")) *font = "TiRo";
			if (!strcmp(tok+1, "Symb")) *font = "Symb";
			if (!strcmp(tok+1, "ZaDb")) *font = "ZaDb";
		}
		else if (!strcmp(tok, "Tf"))
		{
			*size = stack[0];
			top = 0;
		}
		else if (!strcmp(tok, "g"))
		{
			*n = 1;
			color[0] = stack[0];
			top = 0;
		}
		else if (!strcmp(tok, "rg"))
		{
			*n = 3;
			color[0] = stack[0];
			color[1] = stack[1];
			color[2] = stack[2];
			top=0;
		}
		else if (!strcmp(tok, "k"))
		{
			*n = 4;
			color[0] = stack[0];
			color[1] = stack[1];
			color[2] = stack[2];
			color[3] = stack[3];
			top=0;
		}
		else
		{
			float v = fz_strtof(tok, &end);
			if (top < 4)
				stack[top] = v;
			if (*end == 0)
				++top;
			else
				top = 0;
		}
	}
}

void
pdf_print_default_appearance(fz_context *ctx, char *buf, int nbuf, const char *font, float size, int n, const float *color)
{
	if (n == 4)
		fz_snprintf(buf, nbuf, "/%s %g Tf %g %g %g %g k", font, size, color[0], color[1], color[2], color[3]);
	else if (n == 3)
		fz_snprintf(buf, nbuf, "/%s %g Tf %g %g %g rg", font, size, color[0], color[1], color[2]);
	else if (n == 1)
		fz_snprintf(buf, nbuf, "/%s %g Tf %g g", font, size, color[0]);
	else
		fz_snprintf(buf, nbuf, "/%s %g Tf", font, size);
}

void
pdf_annot_default_appearance(fz_context *ctx, pdf_annot *annot, const char **font, float *size, int *n, float color[4])
{
	pdf_obj *da = pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(DA));
	if (!da)
	{
		pdf_obj *trailer = pdf_trailer(ctx, annot->page->doc);
		da = pdf_dict_getl(ctx, trailer, PDF_NAME(Root), PDF_NAME(AcroForm), PDF_NAME(DA), NULL);
	}
	pdf_parse_default_appearance(ctx, pdf_to_str_buf(ctx, da), font, size, n, color);
}

void
pdf_set_annot_default_appearance(fz_context *ctx, pdf_annot *annot, const char *font, float size, int n, const float *color)
{
	char buf[100];

	begin_annot_op(ctx, annot, "Set default appearance");

	fz_try(ctx)
	{
		pdf_print_default_appearance(ctx, buf, sizeof buf, font, size, n, color);

		pdf_dict_put_string(ctx, annot->obj, PDF_NAME(DA), buf, strlen(buf));

		pdf_dict_del(ctx, annot->obj, PDF_NAME(DS)); /* not supported */
		pdf_dict_del(ctx, annot->obj, PDF_NAME(RC)); /* not supported */
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

int pdf_annot_field_flags(fz_context *ctx, pdf_annot *annot)
{
	int ret;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
		ret = pdf_field_flags(ctx, annot->obj);
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

const char *pdf_annot_field_value(fz_context *ctx, pdf_annot *widget)
{
	const char *ret;

	pdf_annot_push_local_xref(ctx, widget);

	fz_try(ctx)
		ret = pdf_field_value(ctx, widget->obj);
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, widget);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

const char *pdf_annot_field_label(fz_context *ctx, pdf_annot *widget)
{
	const char *ret;

	pdf_annot_push_local_xref(ctx, widget);

	fz_try(ctx)
		ret = pdf_field_label(ctx, widget->obj);
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, widget);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return ret;
}

int pdf_set_annot_field_value(fz_context *ctx, pdf_document *doc, pdf_annot *annot, const char *text, int ignore_trigger_events)
{
	int ret;

	begin_annot_op(ctx, annot, "Set field value");

	fz_try(ctx)
	{
		ret = pdf_set_field_value(ctx, doc, annot->obj, text, ignore_trigger_events);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);

	return ret;
}

void
pdf_set_annot_appearance(fz_context *ctx, pdf_annot *annot, const char *appearance, const char *state, fz_matrix ctm, fz_rect bbox, pdf_obj *res, fz_buffer *contents)
{
	pdf_obj *form = NULL;
	pdf_obj *ap, *app;
	pdf_obj *app_name = NULL;

	begin_annot_op(ctx, annot, "Set appearance stream");

	if (!appearance)
		appearance = "N";

	fz_var(form);
	fz_var(app_name);

	fz_try(ctx)
	{
		ap = pdf_dict_get(ctx, annot->obj, PDF_NAME(AP));
		if (!ap)
			ap = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(AP), 1);

		if (!state)
			form = pdf_keep_obj(ctx, pdf_dict_gets(ctx, ap, appearance));
		else
		{
			if (strcmp(appearance, "N") && strcmp(appearance, "R") && strcmp(appearance, "D"))
				fz_throw(ctx, FZ_ERROR_ARGUMENT, "Unknown annotation appearance");

			app_name = pdf_new_name(ctx, appearance);
			app = pdf_dict_get(ctx, ap, app_name);
			if (!app)
				app = pdf_dict_put_dict(ctx, ap, app_name, 2);
			form = pdf_keep_obj(ctx, pdf_dict_gets(ctx, ap, appearance));
		}
		/* Care required here. Some files have multiple annotations, which share
		 * appearance streams. As such, we must NOT reuse such appearance streams.
		 * On the other hand, we cannot afford to always recreate appearance
		 * streams, as this can lead to leakage of partial edits into the document.
		 * Any appearance we generate will be in the incremental section, and we
		 * will never generate shared appearances. As such, we can reuse an
		 * appearance object only if it is in the incremental section. */
		if (!pdf_obj_is_incremental(ctx, form))
		{
			pdf_drop_obj(ctx, form);
			form = NULL;
		}
		if (!pdf_is_dict(ctx, form))
		{
			pdf_drop_obj(ctx, form);
			form = NULL;
			form = pdf_new_xobject(ctx, annot->page->doc, bbox, ctm, res, contents);
		}
		else
			pdf_update_xobject(ctx, annot->page->doc, form, bbox, ctm, res, contents);

		if (!state)
			pdf_dict_puts(ctx, ap, appearance, form);
		else
			pdf_dict_puts(ctx, app, state, form);
		end_annot_op(ctx, annot);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, form);
		pdf_drop_obj(ctx, app_name);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_set_annot_resynthesised(ctx, annot);
}

void
pdf_set_annot_appearance_from_display_list(fz_context *ctx, pdf_annot *annot, const char *appearance, const char *state, fz_matrix ctm, fz_display_list *list)
{
	pdf_document *doc = annot->page->doc;
	fz_device *dev = NULL;
	pdf_obj *res = NULL;
	fz_buffer *contents = NULL;

	/* Convert fitz-space mediabox to pdf-space bbox */
	fz_rect mediabox = fz_bound_display_list(ctx, list);
	fz_matrix transform = { 1, 0, 0, -1, -mediabox.x0, mediabox.y1 };
	fz_rect bbox = fz_transform_rect(mediabox, transform);

	fz_var(dev);
	fz_var(contents);
	fz_var(res);

	begin_annot_op(ctx, annot, "Set appearance stream");

	fz_try(ctx)
	{
		res = pdf_new_dict(ctx, doc, 1);
		contents = fz_new_buffer(ctx, 0);
		dev = pdf_new_pdf_device(ctx, doc, transform, res, contents);
		fz_run_display_list(ctx, list, dev, fz_identity, fz_infinite_rect, NULL);
		fz_close_device(ctx, dev);
		fz_drop_device(ctx, dev);
		dev = NULL;

		pdf_set_annot_appearance(ctx, annot, appearance, state, ctm, bbox, res, contents);
		end_annot_op(ctx, annot);
	}
	fz_always(ctx)
	{
		fz_drop_device(ctx, dev);
		fz_drop_buffer(ctx, contents);
		pdf_drop_obj(ctx, res);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}
}

void pdf_set_annot_stamp_image(fz_context *ctx, pdf_annot *annot, fz_image *img)
{
	pdf_document *doc = annot->page->doc;
	fz_buffer *buf = NULL;
	pdf_obj *res = NULL;
	pdf_obj *res_xobj;

	begin_annot_op(ctx, annot, "Set stamp image");

	fz_var(res);
	fz_var(buf);

	fz_try(ctx)
	{
		// Shrink Rect to fit image, maintaining aspect ratio.
		fz_rect rect = pdf_bound_annot(ctx, annot);
		float s = fz_min((rect.x1 - rect.x0) / img->w, (rect.y1 - rect.y0) / img->h);
		rect.x1 = rect.x0 + img->w * s;
		rect.y1 = rect.y0 + img->h * s;

		// Add image resource
		res = pdf_add_new_dict(ctx, doc, 1);
		res_xobj = pdf_dict_put_dict(ctx, res, PDF_NAME(XObject), 1);
		pdf_dict_put_drop(ctx, res_xobj, PDF_NAME(I), pdf_add_image(ctx, doc, img));

		buf = fz_new_buffer_from_shared_data(ctx, (const unsigned char*)"/I Do\n", 6);

		pdf_set_annot_appearance(ctx, annot, "N", NULL, fz_identity, fz_unit_rect, res, buf);
		pdf_set_annot_rect(ctx, annot, rect);
		end_annot_op(ctx, annot);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		pdf_drop_obj(ctx, res);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}
}

static pdf_obj *filespec_subtypes[] = {
	PDF_NAME(FileAttachment),
	NULL,
};

int
pdf_annot_has_filespec(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype_wrap(ctx, annot, PDF_NAME(FS), filespec_subtypes);
}

pdf_obj *
pdf_annot_filespec(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *filespec;

	pdf_annot_push_local_xref(ctx, annot);

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(FS), filespec_subtypes);
		filespec = pdf_dict_get(ctx, annot->obj, PDF_NAME(FS));
	}
	fz_always(ctx)
		pdf_annot_pop_local_xref(ctx, annot);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return filespec;
}

void
pdf_set_annot_filespec(fz_context *ctx, pdf_annot *annot, pdf_obj *fs)
{
	if (!pdf_is_embedded_file(ctx, fs))
		fz_throw(ctx, FZ_ERROR_ARGUMENT, "cannot set non-filespec as annotation filespec");

	begin_annot_op(ctx, annot, "Set filespec");

	fz_try(ctx)
	{
		check_allowed_subtypes(ctx, annot, PDF_NAME(M), filespec_subtypes);
		pdf_dict_put(ctx, pdf_annot_obj(ctx, annot), PDF_NAME(FS), fs);
		end_annot_op(ctx, annot);
	}
	fz_catch(ctx)
	{
		abandon_annot_op(ctx, annot);
		fz_rethrow(ctx);
	}

	pdf_dirty_annot(ctx, annot);
}

int
pdf_annot_hidden_for_editing(fz_context *ctx, pdf_annot *annot)
{
	return annot->hidden_editing;
}

void
pdf_set_annot_hidden_for_editing(fz_context *ctx, pdf_annot *annot, int hidden)
{
	annot->hidden_editing = hidden;
}
