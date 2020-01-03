#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "../fitz/fitz-imp.h" // ick!

#include <string.h>
#include <time.h>

#ifdef _WIN32
#define timegm _mkgmtime
#endif

#define isdigit(c) (c >= '0' && c <= '9')

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
		pdf_drop_obj(ctx, annot->ap);
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

/* Create transform to fit appearance stream to annotation Rect */
fz_matrix
pdf_annot_transform(fz_context *ctx, pdf_annot *annot)
{
	fz_rect bbox, rect;
	fz_matrix matrix;
	float w, h, x, y;

	rect = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
	bbox = pdf_xobject_bbox(ctx, annot->ap);
	matrix = pdf_xobject_matrix(ctx, annot->ap);

	bbox = fz_transform_rect(bbox, matrix);
	if (bbox.x1 == bbox.x0)
		w = 0;
	else
		w = (rect.x1 - rect.x0) / (bbox.x1 - bbox.x0);
	if (bbox.y1 == bbox.y0)
		h = 0;
	else
		h = (rect.y1 - rect.y0) / (bbox.y1 - bbox.y0);
	x = rect.x0 - bbox.x0;
	y = rect.y0 - bbox.y0;

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
			fz_try(ctx)
			{
				pdf_update_annot(ctx, annot);
				annot->has_new_ap = 0;
			}
			fz_catch(ctx)
				fz_warn(ctx, "could not update appearance for annotation");

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
}

pdf_annot *
pdf_first_annot(fz_context *ctx, pdf_page *page)
{
	return page->annots;
}

pdf_annot *
pdf_next_annot(fz_context *ctx, pdf_annot *annot)
{
	return annot->next;
}

/*
	Return the rectangle for an annotation on a page.
*/
fz_rect
pdf_bound_annot(fz_context *ctx, pdf_annot *annot)
{
	fz_matrix page_ctm;
	fz_rect rect;
	int flags;

	rect = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);

	flags = pdf_dict_get_int(ctx, annot->obj, PDF_NAME(F));
	if (flags & PDF_ANNOT_IS_NO_ROTATE)
	{
		int rotate = pdf_to_int(ctx, pdf_dict_get_inheritable(ctx, annot->page->obj, PDF_NAME(Rotate)));
		fz_point tp = fz_transform_point_xy(rect.x0, rect.y1, page_ctm);
		page_ctm = fz_concat(page_ctm, fz_translate(-tp.x, -tp.y));
		page_ctm = fz_concat(page_ctm, fz_rotate(-rotate));
		page_ctm = fz_concat(page_ctm, fz_translate(tp.x, tp.y));
	}

	return fz_transform_rect(rect, page_ctm);
}

void
pdf_dirty_annot(fz_context *ctx, pdf_annot *annot)
{
	annot->needs_new_ap = 1;
	if (annot->page && annot->page->doc)
		annot->page->doc->dirty = 1;
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
	case PDF_ANNOT_WIDGET: return "Widget";
	case PDF_ANNOT_SCREEN: return "Screen";
	case PDF_ANNOT_PRINTER_MARK: return "PrinterMark";
	case PDF_ANNOT_TRAP_NET: return "TrapNet";
	case PDF_ANNOT_WATERMARK: return "Watermark";
	case PDF_ANNOT_3D: return "3D";
	default: return "UNKNOWN";
	}
}

int
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
	if (!strcmp("Widget", subtype)) return PDF_ANNOT_WIDGET;
	if (!strcmp("Screen", subtype)) return PDF_ANNOT_SCREEN;
	if (!strcmp("PrinterMark", subtype)) return PDF_ANNOT_PRINTER_MARK;
	if (!strcmp("TrapNet", subtype)) return PDF_ANNOT_TRAP_NET;
	if (!strcmp("Watermark", subtype)) return PDF_ANNOT_WATERMARK;
	if (!strcmp("3D", subtype)) return PDF_ANNOT_3D;
	return PDF_ANNOT_UNKNOWN;
}

static int is_allowed_subtype(fz_context *ctx, pdf_annot *annot, pdf_obj *property, pdf_obj **allowed)
{
	pdf_obj *subtype = pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype));
	while (*allowed) {
		if (pdf_name_eq(ctx, subtype, *allowed))
			return 1;
		allowed++;
	}

	return 0;
}

static void check_allowed_subtypes(fz_context *ctx, pdf_annot *annot, pdf_obj *property, pdf_obj **allowed)
{
	pdf_obj *subtype = pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype));
	if (!is_allowed_subtype(ctx, annot, property, allowed))
		fz_throw(ctx, FZ_ERROR_GENERIC, "%s annotations have no %s property", pdf_to_name(ctx, subtype), pdf_to_name(ctx, property));
}

/*
	create a new annotation of the specified type on the
	specified page. The returned pdf_annot structure is owned by the page
	and does not need to be freed.
*/
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
			fz_throw(ctx, FZ_ERROR_GENERIC, "cannot create unknown annotation");

		annot_arr = pdf_dict_get(ctx, page->obj, PDF_NAME(Annots));
		if (annot_arr == NULL)
		{
			annot_arr = pdf_new_array(ctx, doc, 0);
			pdf_dict_put_drop(ctx, page->obj, PDF_NAME(Annots), annot_arr);
		}

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
		annot->ap = NULL;

		/*
			Linking must be done after any call that might throw because
			pdf_drop_annots below actually frees a list. Put the new annot
			at the end of the list, so that it will be drawn last.
		*/
		*page->annot_tailp = annot;
		page->annot_tailp = &annot->next;

		doc->dirty = 1;
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

	return annot;
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

	pdf_annot *annot = pdf_create_annot_raw(ctx, page, type);

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
		}
		break;

	case PDF_ANNOT_FREE_TEXT:
		{
			fz_rect text_rect = { 12, 12, 12+200, 12+100 };

			/* Use undocumented Adobe property to match page rotation. */
			int rot = pdf_to_int(ctx, pdf_dict_get_inheritable(ctx, page->obj, PDF_NAME(Rotate)));
			if (rot != 0)
				pdf_dict_put_int(ctx, annot->obj, PDF_NAME(Rotate), rot);

			pdf_set_annot_rect(ctx, annot, text_rect);
			pdf_set_annot_border(ctx, annot, 0);
			pdf_set_annot_default_appearance(ctx, annot, "Helv", 12, black);
		}
		break;

	case PDF_ANNOT_STAMP:
		{
			fz_rect stamp_rect = { 12, 12, 12+190, 12+50 };
			pdf_set_annot_rect(ctx, annot, stamp_rect);
			pdf_set_annot_color(ctx, annot, 3, red);
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

	return pdf_keep_annot(ctx, annot);
}

void
pdf_delete_annot(fz_context *ctx, pdf_page *page, pdf_annot *annot)
{
	pdf_document *doc = annot->page->doc;
	pdf_annot **annotptr;
	pdf_obj *annot_arr;
	int i;

	if (annot == NULL)
		return;

	/* Remove annot from page's list */
	for (annotptr = &page->annots; *annotptr; annotptr = &(*annotptr)->next)
	{
		if (*annotptr == annot)
			break;
	}

	/* Check the passed annotation was of this page */
	if (*annotptr == NULL)
		return;

	*annotptr = annot->next;

	/* If the removed annotation was the last in the list adjust the end pointer */
	if (*annotptr == NULL)
		page->annot_tailp = annotptr;

	/* Remove the annot from the "Annots" array. */
	annot_arr = pdf_dict_get(ctx, page->obj, PDF_NAME(Annots));
	i = pdf_array_find(ctx, annot_arr, annot->obj);
	if (i >= 0)
		pdf_array_delete(ctx, annot_arr, i);

	/* The garbage collection pass when saving will remove the annot object,
	 * removing it here may break files if multiple pages use the same annot. */

	/* And free it. */
	pdf_drop_annot(ctx, annot);

	doc->dirty = 1;
}

enum pdf_annot_type
pdf_annot_type(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *obj = annot->obj;
	pdf_obj *subtype = pdf_dict_get(ctx, obj, PDF_NAME(Subtype));
	return pdf_annot_type_from_string(ctx, pdf_to_name(ctx, subtype));
}

int
pdf_annot_flags(fz_context *ctx, pdf_annot *annot)
{
	return pdf_dict_get_int(ctx, annot->obj, PDF_NAME(F));
}

void
pdf_set_annot_flags(fz_context *ctx, pdf_annot *annot, int flags)
{
	pdf_dict_put_int(ctx, annot->obj, PDF_NAME(F), flags);
	pdf_dirty_annot(ctx, annot);
}

fz_rect
pdf_annot_rect(fz_context *ctx, pdf_annot *annot)
{
	fz_matrix page_ctm;
	fz_rect annot_rect;
	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	annot_rect = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
	return fz_transform_rect(annot_rect, page_ctm);
}

void
pdf_set_annot_rect(fz_context *ctx, pdf_annot *annot, fz_rect rect)
{
	fz_matrix page_ctm, inv_page_ctm;

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	inv_page_ctm = fz_invert_matrix(page_ctm);
	rect = fz_transform_rect(rect, inv_page_ctm);

	pdf_dict_put_rect(ctx, annot->obj, PDF_NAME(Rect), rect);
	pdf_dirty_annot(ctx, annot);
}

const char *
pdf_annot_contents(fz_context *ctx, pdf_annot *annot)
{
	return pdf_dict_get_text_string(ctx, annot->obj, PDF_NAME(Contents));
}

void
pdf_set_annot_contents(fz_context *ctx, pdf_annot *annot, const char *text)
{
	pdf_dict_put_text_string(ctx, annot->obj, PDF_NAME(Contents), text);
	pdf_dict_del(ctx, annot->obj, PDF_NAME(RC)); /* not supported */
	pdf_dirty_annot(ctx, annot);
}

static pdf_obj *open_subtypes[] = {
	PDF_NAME(Popup),
	PDF_NAME(Text),
	NULL,
};

int
pdf_annot_has_open(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype(ctx, annot, PDF_NAME(Open), open_subtypes);
}

int
pdf_annot_is_open(fz_context *ctx, pdf_annot *annot)
{
	check_allowed_subtypes(ctx, annot, PDF_NAME(Open), open_subtypes);
	return pdf_dict_get_bool(ctx, annot->obj, PDF_NAME(Open));
}

void
pdf_set_annot_is_open(fz_context *ctx, pdf_annot *annot, int is_open)
{
	check_allowed_subtypes(ctx, annot, PDF_NAME(Open), open_subtypes);
	pdf_dict_put_bool(ctx, annot->obj, PDF_NAME(Open), is_open);
	pdf_dirty_annot(ctx, annot);
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
	return is_allowed_subtype(ctx, annot, PDF_NAME(Name), icon_name_subtypes);
}

const char *
pdf_annot_icon_name(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *name;
	check_allowed_subtypes(ctx, annot, PDF_NAME(Name), icon_name_subtypes);
	name = pdf_dict_get(ctx, annot->obj, PDF_NAME(Name));
	if (!name)
	{
		pdf_obj *subtype = pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype));
		if (pdf_name_eq(ctx, subtype, PDF_NAME(Text)))
			return "Note";
		if (pdf_name_eq(ctx, subtype, PDF_NAME(Stamp)))
			return "Draft";
		if (pdf_name_eq(ctx, subtype, PDF_NAME(FileAttachment)))
			return "PushPin";
		if (pdf_name_eq(ctx, subtype, PDF_NAME(Sound)))
			return "Speaker";
	}
	return pdf_to_name(ctx, name);
}

void
pdf_set_annot_icon_name(fz_context *ctx, pdf_annot *annot, const char *name)
{
	check_allowed_subtypes(ctx, annot, PDF_NAME(Name), icon_name_subtypes);
	pdf_dict_put_name(ctx, annot->obj, PDF_NAME(Name), name);
	pdf_dirty_annot(ctx, annot);
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
	return is_allowed_subtype(ctx, annot, PDF_NAME(LE), line_ending_subtypes);
}

void
pdf_annot_line_ending_styles(fz_context *ctx, pdf_annot *annot,
		enum pdf_line_ending *start_style,
		enum pdf_line_ending *end_style)
{
	pdf_obj *style;
	check_allowed_subtypes(ctx, annot, PDF_NAME(LE), line_ending_subtypes);
	style = pdf_dict_get(ctx, annot->obj, PDF_NAME(LE));
	*start_style = pdf_line_ending_from_name(ctx, pdf_array_get(ctx, style, 0));
	*end_style = pdf_line_ending_from_name(ctx, pdf_array_get(ctx, style, 1));
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
	pdf_document *doc = annot->page->doc;
	pdf_obj *style;
	check_allowed_subtypes(ctx, annot, PDF_NAME(LE), line_ending_subtypes);
	style = pdf_new_array(ctx, doc, 2);
	pdf_dict_put_drop(ctx, annot->obj, PDF_NAME(LE), style);
	pdf_array_put_drop(ctx, style, 0, pdf_name_from_line_ending(ctx, start_style));
	pdf_array_put_drop(ctx, style, 1, pdf_name_from_line_ending(ctx, end_style));
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
	bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
	bs_w = pdf_dict_get(ctx, bs, PDF_NAME(W));
	if (pdf_is_number(ctx, bs_w))
		return pdf_to_real(ctx, bs_w);
	border = pdf_dict_get(ctx, annot->obj, PDF_NAME(Border));
	bs_w = pdf_array_get(ctx, border, 2);
	if (pdf_is_number(ctx, bs_w))
		return pdf_to_real(ctx, bs_w);
	return 1;
}

void
pdf_set_annot_border(fz_context *ctx, pdf_annot *annot, float w)
{
	pdf_obj *bs = pdf_dict_get(ctx, annot->obj, PDF_NAME(BS));
	if (!pdf_is_dict(ctx, bs))
		bs = pdf_dict_put_dict(ctx, annot->obj, PDF_NAME(BS), 1);
	pdf_dict_put_real(ctx, bs, PDF_NAME(W), w);

	pdf_dict_del(ctx, annot->obj, PDF_NAME(Border)); /* deprecated */
	pdf_dict_del(ctx, annot->obj, PDF_NAME(BE)); /* not supported */

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
	pdf_dict_put_int(ctx, annot->obj, PDF_NAME(Q), q);
	pdf_dirty_annot(ctx, annot);
}

float pdf_annot_opacity(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *ca = pdf_dict_get(ctx, annot->obj, PDF_NAME(CA));
	if (pdf_is_number(ctx, ca))
		return pdf_to_real(ctx, ca);
	return 1;
}

void pdf_set_annot_opacity(fz_context *ctx, pdf_annot *annot, float opacity)
{
	if (opacity != 1)
		pdf_dict_put_real(ctx, annot->obj, PDF_NAME(CA), opacity);
	else
		pdf_dict_del(ctx, annot->obj, PDF_NAME(CA));
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

static void pdf_set_annot_color_imp(fz_context *ctx, pdf_annot *annot, pdf_obj *key, int n, const float color[4], pdf_obj **allowed)
{
	pdf_document *doc = annot->page->doc;
	pdf_obj *arr;

	if (allowed)
		check_allowed_subtypes(ctx, annot, key, allowed);
	if (n != 0 && n != 1 && n != 3 && n != 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "color must be 0, 1, 3 or 4 components");
	if (!color)
		fz_throw(ctx, FZ_ERROR_GENERIC, "no color given");

	arr = pdf_new_array(ctx, doc, n);
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
	{
		pdf_drop_obj(ctx, arr);
		fz_rethrow(ctx);
	}

	pdf_dict_put_drop(ctx, annot->obj, key, arr);
	pdf_dirty_annot(ctx, annot);
}

void
pdf_annot_color(fz_context *ctx, pdf_annot *annot, int *n, float color[4])
{
	pdf_obj *c = pdf_dict_get(ctx, annot->obj, PDF_NAME(C));
	pdf_annot_color_imp(ctx, c, n, color);
}

void
pdf_annot_MK_BG(fz_context *ctx, pdf_annot *annot, int *n, float color[4])
{
	pdf_obj *mk_bg = pdf_dict_get(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(BG));
	pdf_annot_color_imp(ctx, mk_bg, n, color);
}

int
pdf_annot_MK_BG_rgb(fz_context *ctx, pdf_annot *annot, float rgb[3])
{
	pdf_obj *mk_bg = pdf_dict_get(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(BG));
	return pdf_annot_color_rgb(ctx, mk_bg, rgb);
}

void
pdf_annot_MK_BC(fz_context *ctx, pdf_annot *annot, int *n, float color[4])
{
	pdf_obj *mk_bc = pdf_dict_get(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(BC));
	pdf_annot_color_imp(ctx, mk_bc, n, color);
}

int
pdf_annot_MK_BC_rgb(fz_context *ctx, pdf_annot *annot, float rgb[3])
{
	pdf_obj *mk_bc = pdf_dict_get(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME(MK)), PDF_NAME(BC));
	return pdf_annot_color_rgb(ctx, mk_bc, rgb);
}

void
pdf_set_annot_color(fz_context *ctx, pdf_annot *annot, int n, const float color[4])
{
	pdf_set_annot_color_imp(ctx, annot, PDF_NAME(C), n, color, NULL);
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
	return is_allowed_subtype(ctx, annot, PDF_NAME(IC), interior_color_subtypes);
}

void
pdf_annot_interior_color(fz_context *ctx, pdf_annot *annot, int *n, float color[4])
{
	pdf_obj *ic = pdf_dict_get(ctx, annot->obj, PDF_NAME(IC));
	pdf_annot_color_imp(ctx, ic, n, color);
}

void
pdf_set_annot_interior_color(fz_context *ctx, pdf_annot *annot, int n, const float color[4])
{
	pdf_set_annot_color_imp(ctx, annot, PDF_NAME(IC), n, color, interior_color_subtypes);
}

static pdf_obj *line_subtypes[] = {
	PDF_NAME(Line),
	NULL,
};

int
pdf_annot_has_line(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype(ctx, annot, PDF_NAME(L), line_subtypes);
}

void
pdf_annot_line(fz_context *ctx, pdf_annot *annot, fz_point *a, fz_point *b)
{
	fz_matrix page_ctm;
	pdf_obj *line;

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

void
pdf_set_annot_line(fz_context *ctx, pdf_annot *annot, fz_point a, fz_point b)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *line;

	check_allowed_subtypes(ctx, annot, PDF_NAME(L), line_subtypes);

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	inv_page_ctm = fz_invert_matrix(page_ctm);

	a = fz_transform_point(a, inv_page_ctm);
	b = fz_transform_point(b, inv_page_ctm);

	line = pdf_new_array(ctx, annot->page->doc, 4);
	pdf_dict_put_drop(ctx, annot->obj, PDF_NAME(L), line);
	pdf_array_push_real(ctx, line, a.x);
	pdf_array_push_real(ctx, line, a.y);
	pdf_array_push_real(ctx, line, b.x);
	pdf_array_push_real(ctx, line, b.y);

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
	return is_allowed_subtype(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);
}

int
pdf_annot_vertex_count(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *vertices;
	check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);
	vertices = pdf_dict_get(ctx, annot->obj, PDF_NAME(Vertices));
	return pdf_array_len(ctx, vertices) / 2;
}

fz_point
pdf_annot_vertex(fz_context *ctx, pdf_annot *annot, int i)
{
	pdf_obj *vertices;
	fz_matrix page_ctm;
	fz_point point;

	check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);

	vertices = pdf_dict_get(ctx, annot->obj, PDF_NAME(Vertices));

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);

	point.x = pdf_array_get_real(ctx, vertices, i * 2);
	point.y = pdf_array_get_real(ctx, vertices, i * 2 + 1);
	return fz_transform_point(point, page_ctm);
}

void
pdf_set_annot_vertices(fz_context *ctx, pdf_annot *annot, int n, const fz_point *v)
{
	pdf_document *doc = annot->page->doc;
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *vertices;
	fz_point point;
	int i;

	check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);
	if (n <= 0 || !v)
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid number of vertices");

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	inv_page_ctm = fz_invert_matrix(page_ctm);

	vertices = pdf_new_array(ctx, doc, n * 2);
	for (i = 0; i < n; ++i)
	{
		point = fz_transform_point(v[i], inv_page_ctm);
		pdf_array_push_real(ctx, vertices, point.x);
		pdf_array_push_real(ctx, vertices, point.y);
	}
	pdf_dict_put_drop(ctx, annot->obj, PDF_NAME(Vertices), vertices);
	pdf_dirty_annot(ctx, annot);
}

void pdf_clear_annot_vertices(fz_context *ctx, pdf_annot *annot)
{
	check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);
	pdf_dict_del(ctx, annot->obj, PDF_NAME(Vertices));
	pdf_dirty_annot(ctx, annot);
}

void pdf_add_annot_vertex(fz_context *ctx, pdf_annot *annot, fz_point p)
{
	pdf_document *doc = annot->page->doc;
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *vertices;

	check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	inv_page_ctm = fz_invert_matrix(page_ctm);

	vertices = pdf_dict_get(ctx, annot->obj, PDF_NAME(Vertices));
	if (!pdf_is_array(ctx, vertices))
	{
		vertices = pdf_new_array(ctx, doc, 32);
		pdf_dict_put_drop(ctx, annot->obj, PDF_NAME(Vertices), vertices);
	}

	p = fz_transform_point(p, inv_page_ctm);
	pdf_array_push_real(ctx, vertices, p.x);
	pdf_array_push_real(ctx, vertices, p.y);

	pdf_dirty_annot(ctx, annot);
}

void pdf_set_annot_vertex(fz_context *ctx, pdf_annot *annot, int i, fz_point p)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *vertices;

	check_allowed_subtypes(ctx, annot, PDF_NAME(Vertices), vertices_subtypes);

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	inv_page_ctm = fz_invert_matrix(page_ctm);

	p = fz_transform_point(p, inv_page_ctm);

	vertices = pdf_dict_get(ctx, annot->obj, PDF_NAME(Vertices));
	pdf_array_put_drop(ctx, vertices, i * 2 + 0, pdf_new_real(ctx, p.x));
	pdf_array_put_drop(ctx, vertices, i * 2 + 1, pdf_new_real(ctx, p.y));
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
	return is_allowed_subtype(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);
}

int
pdf_annot_quad_point_count(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *quad_points;
	check_allowed_subtypes(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);
	quad_points = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
	return pdf_array_len(ctx, quad_points) / 8;
}

fz_quad
pdf_annot_quad_point(fz_context *ctx, pdf_annot *annot, int idx)
{
	pdf_obj *quad_points;
	fz_matrix page_ctm;
	float v[8];
	int i;

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

	return fz_make_quad(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
}

void
pdf_set_annot_quad_points(fz_context *ctx, pdf_annot *annot, int n, const fz_quad *q)
{
	pdf_document *doc = annot->page->doc;
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *quad_points;
	fz_quad quad;
	int i;

	check_allowed_subtypes(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);
	if (n <= 0 || !q)
		fz_throw(ctx, FZ_ERROR_GENERIC, "invalid number of quadrilaterals");

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	inv_page_ctm = fz_invert_matrix(page_ctm);

	quad_points = pdf_new_array(ctx, doc, n);
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
	pdf_dict_put_drop(ctx, annot->obj, PDF_NAME(QuadPoints), quad_points);
	pdf_dirty_annot(ctx, annot);
}

void
pdf_clear_annot_quad_points(fz_context *ctx, pdf_annot *annot)
{
	check_allowed_subtypes(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);
	pdf_dict_del(ctx, annot->obj, PDF_NAME(QuadPoints));
	pdf_dirty_annot(ctx, annot);
}

void
pdf_add_annot_quad_point(fz_context *ctx, pdf_annot *annot, fz_quad quad)
{
	pdf_document *doc = annot->page->doc;
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *quad_points;

	check_allowed_subtypes(ctx, annot, PDF_NAME(QuadPoints), quad_point_subtypes);

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	inv_page_ctm = fz_invert_matrix(page_ctm);

	quad_points = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
	if (!pdf_is_array(ctx, quad_points))
	{
		quad_points = pdf_new_array(ctx, doc, 8);
		pdf_dict_put_drop(ctx, annot->obj, PDF_NAME(QuadPoints), quad_points);
	}

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

	pdf_dirty_annot(ctx, annot);
}

static pdf_obj *ink_list_subtypes[] = {
	PDF_NAME(Ink),
	NULL,
};

int
pdf_annot_has_ink_list(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);
}

int
pdf_annot_ink_list_count(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *ink_list;
	check_allowed_subtypes(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);
	ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
	return pdf_array_len(ctx, ink_list);
}

int
pdf_annot_ink_list_stroke_count(fz_context *ctx, pdf_annot *annot, int i)
{
	pdf_obj *ink_list;
	pdf_obj *stroke;
	check_allowed_subtypes(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);
	ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
	stroke = pdf_array_get(ctx, ink_list, i);
	return pdf_array_len(ctx, stroke) / 2;
}

fz_point
pdf_annot_ink_list_stroke_vertex(fz_context *ctx, pdf_annot *annot, int i, int k)
{
	pdf_obj *ink_list;
	pdf_obj *stroke;
	fz_matrix page_ctm;
	fz_point point;

	check_allowed_subtypes(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);

	ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
	stroke = pdf_array_get(ctx, ink_list, i);

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);

	point.x = pdf_array_get_real(ctx, stroke, k * 2 + 0);
	point.y = pdf_array_get_real(ctx, stroke, k * 2 + 1);
	return fz_transform_point(point, page_ctm);
}

void
pdf_set_annot_ink_list(fz_context *ctx, pdf_annot *annot, int n, const int *count, const fz_point *v)
{
	pdf_document *doc = annot->page->doc;
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *ink_list, *stroke;
	fz_point point;
	int i, k;

	check_allowed_subtypes(ctx, annot, PDF_NAME(InkList), ink_list_subtypes);

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	inv_page_ctm = fz_invert_matrix(page_ctm);

	// TODO: update Rect (in update appearance perhaps?)

	ink_list = pdf_new_array(ctx, doc, n);
	for (i = 0; i < n; ++i)
	{
		stroke = pdf_new_array(ctx, doc, count[i] * 2);
		for (k = 0; k < count[i]; ++k)
		{
			point = fz_transform_point(*v++, inv_page_ctm);
			pdf_array_push_real(ctx, stroke, point.x);
			pdf_array_push_real(ctx, stroke, point.y);
		}
		pdf_array_push_drop(ctx, ink_list, stroke);
	}
	pdf_dict_put_drop(ctx, annot->obj, PDF_NAME(InkList), ink_list);
	pdf_dirty_annot(ctx, annot);
}

void
pdf_clear_annot_ink_list(fz_context *ctx, pdf_annot *annot)
{
	pdf_dict_del(ctx, annot->obj, PDF_NAME(InkList));
	pdf_dirty_annot(ctx, annot);
}

void pdf_add_annot_ink_list_stroke(fz_context *ctx, pdf_annot *annot)
{
	pdf_obj *ink_list;

	ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
	if (!pdf_is_array(ctx, ink_list))
		ink_list = pdf_dict_put_array(ctx, annot->obj, PDF_NAME(InkList), 10);

	pdf_array_push_array(ctx, ink_list, 16);

	pdf_dirty_annot(ctx, annot);
}

void pdf_add_annot_ink_list_stroke_vertex(fz_context *ctx, pdf_annot *annot, fz_point p)
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *ink_list, *stroke;

	pdf_page_transform(ctx, annot->page, NULL, &page_ctm);
	inv_page_ctm = fz_invert_matrix(page_ctm);

	ink_list = pdf_dict_get(ctx, annot->obj, PDF_NAME(InkList));
	stroke = pdf_array_get(ctx, ink_list, pdf_array_len(ctx, ink_list)-1);

	p = fz_transform_point(p, inv_page_ctm);
	pdf_array_push_real(ctx, stroke, p.x);
	pdf_array_push_real(ctx, stroke, p.y);

	pdf_dirty_annot(ctx, annot);
}

void
pdf_add_annot_ink_list(fz_context *ctx, pdf_annot *annot, int n, fz_point p[])
{
	fz_matrix page_ctm, inv_page_ctm;
	pdf_obj *ink_list, *stroke;
	int i;

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

	pdf_dirty_annot(ctx, annot);
}

void
pdf_format_date(fz_context *ctx, char *s, int n, int64_t isecs)
{
	time_t secs = isecs;
#ifdef _POSIX_SOURCE
	struct tm tmbuf, *tm = gmtime_r(&secs, &tmbuf);
#else
	struct tm *tm = gmtime(&secs);
#endif
	if (!tm)
	{
		fz_strlcpy(s, "D:19700101000000Z", n);
	}
	else
	{
		if (!strftime(s, n, "D:%Y%m%d%H%M%SZ", tm) && n > 0)
			s[0] = '\0';
	}
}

static int64_t
pdf_parse_date(fz_context *ctx, const char *s)
{
	int tz_sign, tz_hour, tz_min, tz_adj;
	struct tm tm;
	time_t utc;

	if (!s)
		return 0;

	memset(&tm, 0, sizeof tm);
	tm.tm_mday = 1;

	tz_sign = 1;
	tz_hour = 0;
	tz_min = 0;

	if (s[0] == 'D' && s[1] == ':')
		s += 2;

	if (!isdigit(s[0]) || !isdigit(s[1]) || !isdigit(s[2]) || !isdigit(s[3]))
	{
		fz_warn(ctx, "invalid date format (missing year)");
		return 0;
	}
	tm.tm_year = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0') - 1900;
	s += 4;

	if (isdigit(s[0]) && isdigit(s[1]))
	{
		tm.tm_mon = (s[0]-'0')*10 + (s[1]-'0') - 1; /* month is 0-11 in struct tm */
		s += 2;
		if (isdigit(s[0]) && isdigit(s[1]))
		{
			tm.tm_mday = (s[0]-'0')*10 + (s[1]-'0');
			s += 2;
			if (isdigit(s[0]) && isdigit(s[1]))
			{
				tm.tm_hour = (s[0]-'0')*10 + (s[1]-'0');
				s += 2;
				if (isdigit(s[0]) && isdigit(s[1]))
				{
					tm.tm_min = (s[0]-'0')*10 + (s[1]-'0');
					s += 2;
					if (isdigit(s[0]) && isdigit(s[1]))
					{
						tm.tm_sec = (s[0]-'0')*10 + (s[1]-'0');
						s += 2;
					}
				}
			}
		}
	}

	if (s[0] == 'Z')
	{
		s += 1;
	}
	else if ((s[0] == '-' || s[0] == '+') && isdigit(s[1]) && isdigit(s[2]))
	{
		tz_sign = (s[0] == '-') ? -1 : 1;
		tz_hour = (s[1]-'0')*10 + (s[2]-'0');
		s += 3;
		if (s[0] == '\'' && isdigit(s[1]) && isdigit(s[2]))
		{
			tz_min = (s[1]-'0')*10 + (s[2]-'0');
			s += 3;
			if (s[0] == '\'')
				s += 1;
		}
	}

	if (s[0] != 0)
		fz_warn(ctx, "invalid date format (garbage at end)");

	utc = timegm(&tm);
	if (utc == (time_t)-1)
	{
		fz_warn(ctx, "date overflow error");
		return 0;
	}

	tz_adj = tz_sign * (tz_hour * 3600 + tz_min * 60);
	return utc - tz_adj;
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
	pdf_obj *date = pdf_dict_get(ctx, annot->obj, PDF_NAME(M));
	return date ? pdf_parse_date(ctx, pdf_to_str_buf(ctx, date)) : 0;
}

/*
	Set annotation's modification date in seconds since the epoch.
*/
void
pdf_set_annot_modification_date(fz_context *ctx, pdf_annot *annot, int64_t secs)
{
	char s[40];

	check_allowed_subtypes(ctx, annot, PDF_NAME(M), markup_subtypes);

	pdf_format_date(ctx, s, sizeof s, secs);
	pdf_dict_put_string(ctx, annot->obj, PDF_NAME(M), s, strlen(s));
	pdf_dirty_annot(ctx, annot);
}

int
pdf_annot_has_author(fz_context *ctx, pdf_annot *annot)
{
	return is_allowed_subtype(ctx, annot, PDF_NAME(T), markup_subtypes);
}

const char *
pdf_annot_author(fz_context *ctx, pdf_annot *annot)
{
	check_allowed_subtypes(ctx, annot, PDF_NAME(T), markup_subtypes);
	return pdf_dict_get_text_string(ctx, annot->obj, PDF_NAME(T));
}

void
pdf_set_annot_author(fz_context *ctx, pdf_annot *annot, const char *author)
{
	check_allowed_subtypes(ctx, annot, PDF_NAME(T), markup_subtypes);
	pdf_dict_put_text_string(ctx, annot->obj, PDF_NAME(T), author);
	pdf_dirty_annot(ctx, annot);
}

void
pdf_parse_default_appearance(fz_context *ctx, const char *da, const char **font, float *size, float color[3])
{
	char buf[100], *p = buf, *tok, *end;
	float stack[3] = { 0, 0, 0 };
	int top = 0;

	*font = "Helv";
	*size = 12;
	color[0] = color[1] = color[2] = 0;

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
			color[0] = color[1] = color[2] = stack[0];
			top = 0;
		}
		else if (!strcmp(tok, "rg"))
		{
			color[0] = stack[0];
			color[1] = stack[1];
			color[2] = stack[2];
			top=0;
		}
		else
		{
			if (top < 3)
				stack[top] = fz_strtof(tok, &end);
			if (*end == 0)
				++top;
			else
				top = 0;
		}
	}
}

void
pdf_print_default_appearance(fz_context *ctx, char *buf, int nbuf, const char *font, float size, const float color[3])
{
	if (color[0] > 0 || color[1] > 0 || color[2] > 0)
		fz_snprintf(buf, nbuf, "/%s %g Tf %g %g %g rg", font, size, color[0], color[1], color[2]);
	else
		fz_snprintf(buf, nbuf, "/%s %g Tf", font, size);
}

void
pdf_annot_default_appearance(fz_context *ctx, pdf_annot *annot, const char **font, float *size, float color[3])
{
	pdf_obj *da = pdf_dict_get_inheritable(ctx, annot->obj, PDF_NAME(DA));
	if (!da)
	{
		pdf_obj *trailer = pdf_trailer(ctx, annot->page->doc);
		da = pdf_dict_getl(ctx, trailer, PDF_NAME(Root), PDF_NAME(AcroForm), PDF_NAME(DA), NULL);
	}
	pdf_parse_default_appearance(ctx, pdf_to_str_buf(ctx, da), font, size, color);
}

void
pdf_set_annot_default_appearance(fz_context *ctx, pdf_annot *annot, const char *font, float size, const float color[3])
{
	char buf[100];

	pdf_print_default_appearance(ctx, buf, sizeof buf, font, size, color);

	pdf_dict_put_string(ctx, annot->obj, PDF_NAME(DA), buf, strlen(buf));

	pdf_dict_del(ctx, annot->obj, PDF_NAME(DS)); /* not supported */
	pdf_dict_del(ctx, annot->obj, PDF_NAME(RC)); /* not supported */

	pdf_dirty_annot(ctx, annot);
}
