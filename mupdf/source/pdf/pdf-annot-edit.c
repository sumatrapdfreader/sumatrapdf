#include "mupdf/pdf.h"

#define TEXT_ANNOT_SIZE (25.0)

static const char *annot_type_str(fz_annot_type type)
{
	switch (type)
	{
	case FZ_ANNOT_TEXT: return "Text";
	case FZ_ANNOT_LINK: return "Link";
	case FZ_ANNOT_FREETEXT: return "FreeText";
	case FZ_ANNOT_LINE: return "Line";
	case FZ_ANNOT_SQUARE: return "Square";
	case FZ_ANNOT_CIRCLE: return "Circle";
	case FZ_ANNOT_POLYGON: return "Polygon";
	case FZ_ANNOT_POLYLINE: return "PolyLine";
	case FZ_ANNOT_HIGHLIGHT: return "Highlight";
	case FZ_ANNOT_UNDERLINE: return "Underline";
	case FZ_ANNOT_SQUIGGLY: return "Squiggly";
	case FZ_ANNOT_STRIKEOUT: return "StrikeOut";
	case FZ_ANNOT_STAMP: return "Stamp";
	case FZ_ANNOT_CARET: return "Caret";
	case FZ_ANNOT_INK: return "Ink";
	case FZ_ANNOT_POPUP: return "Popup";
	case FZ_ANNOT_FILEATTACHMENT: return "FileAttachment";
	case FZ_ANNOT_SOUND: return "Sound";
	case FZ_ANNOT_MOVIE: return "Movie";
	case FZ_ANNOT_WIDGET: return "Widget";
	case FZ_ANNOT_SCREEN: return "Screen";
	case FZ_ANNOT_PRINTERMARK: return "PrinterMark";
	case FZ_ANNOT_TRAPNET: return "TrapNet";
	case FZ_ANNOT_WATERMARK: return "Watermark";
	case FZ_ANNOT_3D: return "3D";
	default: return "";
	}
}

void
pdf_update_annot(pdf_document *doc, pdf_annot *annot)
{
	/* SumatraPDF: prevent regressions */
#if 0
	pdf_obj *obj, *ap, *as, *n;
	fz_context *ctx = doc->ctx;

	if (doc->update_appearance)
		doc->update_appearance(doc, annot);

	obj = annot->obj;

	ap = pdf_dict_gets(obj, "AP");
	as = pdf_dict_gets(obj, "AS");

	if (pdf_is_dict(ap))
	{
		pdf_hotspot *hp = &doc->hotspot;

		n = NULL;

		if (hp->num == pdf_to_num(obj)
			&& hp->gen == pdf_to_gen(obj)
			&& (hp->state & HOTSPOT_POINTER_DOWN))
		{
			n = pdf_dict_gets(ap, "D"); /* down state */
		}

		if (n == NULL)
			n = pdf_dict_gets(ap, "N"); /* normal state */

		/* lookup current state in sub-dictionary */
		if (!pdf_is_stream(doc, pdf_to_num(n), pdf_to_gen(n)))
			n = pdf_dict_get(n, as);

		pdf_drop_xobject(ctx, annot->ap);
		annot->ap = NULL;

		if (pdf_is_stream(doc, pdf_to_num(n), pdf_to_gen(n)))
		{
			fz_try(ctx)
			{
				annot->ap = pdf_load_xobject(doc, n);
				pdf_transform_annot(annot);
				annot->ap_iteration = annot->ap->iteration;
			}
			fz_catch(ctx)
			{
				fz_rethrow_if(ctx, FZ_ERROR_TRYLATER);
				fz_warn(ctx, "ignoring broken annotation");
			}
		}
	}
#endif
}

pdf_annot *
pdf_create_annot(pdf_document *doc, pdf_page *page, fz_annot_type type)
{
	fz_context *ctx = doc->ctx;
	pdf_annot *annot = NULL;
	pdf_obj *annot_obj = pdf_new_dict(doc, 0);
	pdf_obj *ind_obj = NULL;

	fz_var(annot);
	fz_var(ind_obj);
	fz_try(ctx)
	{
		int ind_obj_num;
		fz_rect rect = {0.0, 0.0, 0.0, 0.0};
		const char *type_str = annot_type_str(type);
		pdf_obj *annot_arr = pdf_dict_gets(page->me, "Annots");
		if (annot_arr == NULL)
		{
			annot_arr = pdf_new_array(doc, 0);
			pdf_dict_puts_drop(page->me, "Annots", annot_arr);
		}

		pdf_dict_puts_drop(annot_obj, "Type", pdf_new_name(doc, "Annot"));

		pdf_dict_puts_drop(annot_obj, "Subtype", pdf_new_name(doc, type_str));
		pdf_dict_puts_drop(annot_obj, "Rect", pdf_new_rect(doc, &rect));

		/* Make printable as default */
		pdf_dict_puts_drop(annot_obj, "F", pdf_new_int(doc, F_Print));

		annot = fz_malloc_struct(ctx, pdf_annot);
		annot->page = page;
		annot->rect = rect;
		annot->pagerect = rect;
		annot->ap = NULL;
		annot->widget_type = PDF_WIDGET_TYPE_NOT_WIDGET;
		annot->annot_type = type;

		/*
			Both annotation object and annotation structure are now created.
			Insert the object in the hierarchy and the structure in the
			page's array.
		*/
		ind_obj_num = pdf_create_object(doc);
		pdf_update_object(doc, ind_obj_num, annot_obj);
		ind_obj = pdf_new_indirect(doc, ind_obj_num, 0);
		pdf_array_push(annot_arr, ind_obj);
		annot->obj = pdf_keep_obj(ind_obj);

		/*
			Linking must be done after any call that might throw because
			pdf_free_annot below actually frees a list. Put the new annot
			at the end of the list, so that it will be drawn last.
		*/
		*page->annot_tailp = annot;
		page->annot_tailp = &annot->next;

		doc->dirty = 1;
	}
	fz_always(ctx)
	{
		pdf_drop_obj(annot_obj);
		pdf_drop_obj(ind_obj);
	}
	fz_catch(ctx)
	{
		pdf_free_annot(ctx, annot);
		fz_rethrow(ctx);
	}

	return annot;
}

void
pdf_delete_annot(pdf_document *doc, pdf_page *page, pdf_annot *annot)
{
	fz_context *ctx = doc->ctx;
	pdf_annot **annotptr;
	pdf_obj *old_annot_arr;
	pdf_obj *annot_arr;

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

	/* Stick it in the deleted list */
	annot->next = page->deleted_annots;
	page->deleted_annots = annot;

	pdf_drop_xobject(ctx, annot->ap);
	annot->ap = NULL;

	/* Recreate the "Annots" array with this annot removed */
	old_annot_arr = pdf_dict_gets(page->me, "Annots");

	if (old_annot_arr)
	{
		int i, n = pdf_array_len(old_annot_arr);
		annot_arr = pdf_new_array(doc, n?(n-1):0);

		fz_try(ctx)
		{
			for (i = 0; i < n; i++)
			{
				pdf_obj *obj = pdf_array_get(old_annot_arr, i);

				if (obj != annot->obj)
					pdf_array_push(annot_arr, obj);
			}

			if (pdf_is_indirect(old_annot_arr))
				pdf_update_object(doc, pdf_to_num(old_annot_arr), annot_arr);
			else
				pdf_dict_puts(page->me, "Annots", annot_arr);

			if (pdf_is_indirect(annot->obj))
				pdf_delete_object(doc, pdf_to_num(annot->obj));
		}
		fz_always(ctx)
		{
			pdf_drop_obj(annot_arr);
		}
		fz_catch(ctx)
		{
			fz_rethrow(ctx);
		}
	}

	pdf_drop_obj(annot->obj);
	annot->obj = NULL;
	doc->dirty = 1;
}

void
pdf_set_markup_annot_quadpoints(pdf_document *doc, pdf_annot *annot, fz_point *qp, int n)
{
	fz_matrix ctm;
	pdf_obj *arr = pdf_new_array(doc, n*2);
	int i;

	fz_invert_matrix(&ctm, &annot->page->ctm);

	pdf_dict_puts_drop(annot->obj, "QuadPoints", arr);

	for (i = 0; i < n; i++)
	{
		fz_point pt = qp[i];
		pdf_obj *r;

		fz_transform_point(&pt, &ctm);
		r = pdf_new_real(doc, pt.x);
		pdf_array_push_drop(arr, r);
		r = pdf_new_real(doc, pt.y);
		pdf_array_push_drop(arr, r);
	}
}

static void update_rect(fz_context *ctx, pdf_annot *annot)
{
	pdf_to_rect(ctx, pdf_dict_gets(annot->obj, "Rect"), &annot->rect);
	annot->pagerect = annot->rect;
	fz_transform_rect(&annot->pagerect, &annot->page->ctm);
}

void
pdf_set_ink_annot_list(pdf_document *doc, pdf_annot *annot, fz_point *pts, int *counts, int ncount, float color[3], float thickness)
{
	fz_context *ctx = doc->ctx;
	fz_matrix ctm;
	pdf_obj *list = pdf_new_array(doc, ncount);
	pdf_obj *bs, *col;
	fz_rect rect;
	int i, k = 0;

	fz_invert_matrix(&ctm, &annot->page->ctm);

	pdf_dict_puts_drop(annot->obj, "InkList", list);

	for (i = 0; i < ncount; i++)
	{
		int j;
		pdf_obj *arc = pdf_new_array(doc, counts[i]);

		pdf_array_push_drop(list, arc);

		for (j = 0; j < counts[i]; j++)
		{
			fz_point pt = pts[k];

			fz_transform_point(&pt, &ctm);

			if (i == 0 && j == 0)
			{
				rect.x0 = rect.x1 = pt.x;
				rect.y0 = rect.y1 = pt.y;
			}
			else
			{
				fz_include_point_in_rect(&rect, &pt);
			}

			pdf_array_push_drop(arc, pdf_new_real(doc, pt.x));
			pdf_array_push_drop(arc, pdf_new_real(doc, pt.y));
			k++;
		}
	}

	/*
		Expand the rectangle by thickness all around. We cannot use
		fz_expand_rect because the rectangle might be empty in the
		single point case
	*/
	if (k > 0)
	{
		rect.x0 -= thickness;
		rect.y0 -= thickness;
		rect.x1 += thickness;
		rect.y1 += thickness;
	}

	pdf_dict_puts_drop(annot->obj, "Rect", pdf_new_rect(doc, &rect));
	update_rect(ctx, annot);

	bs = pdf_new_dict(doc, 1);
	pdf_dict_puts_drop(annot->obj, "BS", bs);
	pdf_dict_puts_drop(bs, "W", pdf_new_real(doc, thickness));

	col = pdf_new_array(doc, 3);
	pdf_dict_puts_drop(annot->obj, "C", col);
	for (i = 0; i < 3; i++)
		pdf_array_push_drop(col, pdf_new_real(doc, color[i]));
}

static void find_free_font_name(pdf_obj *fdict, char *buf, int buf_size)
{
	int i;

	/* Find a number X such that /FX doesn't occur as a key in fdict */
	for (i = 0; 1; i++)
	{
		snprintf(buf, buf_size, "F%d", i);

		if (!pdf_dict_gets(fdict, buf))
			break;
	}
}

void pdf_set_text_annot_position(pdf_document *doc, pdf_annot *annot, fz_point pt)
{
	fz_matrix ctm;
	fz_rect rect;
	int flags;

	fz_invert_matrix(&ctm, &annot->page->ctm);
	rect.x0 = pt.x;
	rect.x1 = pt.x + TEXT_ANNOT_SIZE;
	rect.y0 = pt.y;
	rect.y1 = pt.y + TEXT_ANNOT_SIZE;
	fz_transform_rect(&rect, &ctm);

	pdf_dict_puts_drop(annot->obj, "Rect", pdf_new_rect(doc, &rect));

	flags = pdf_to_int(pdf_dict_gets(annot->obj, "F"));
	flags |= (F_NoZoom|F_NoRotate);
	pdf_dict_puts_drop(annot->obj, "F", pdf_new_int(doc, flags));

	update_rect(doc->ctx, annot);
}

void pdf_set_annot_contents(pdf_document *doc, pdf_annot *annot, char *text)
{
	pdf_dict_puts_drop(annot->obj, "Contents", pdf_new_string(doc, text, strlen(text)));
}

char *pdf_annot_contents(pdf_document *doc, pdf_annot *annot)
{
	return pdf_to_str_buf(pdf_dict_getp(annot->obj, "Contents"));
}

void pdf_set_free_text_details(pdf_document *doc, pdf_annot *annot, fz_point *pos, char *text, char *font_name, float font_size, float color[3])
{
	fz_context *ctx = doc->ctx;
	char nbuf[32];
	pdf_obj *dr;
	pdf_obj *form_fonts;
	pdf_obj *font = NULL;
	pdf_obj *ref;
	pdf_font_desc *font_desc = NULL;
	pdf_da_info da_info;
	fz_buffer *fzbuf = NULL;
	fz_matrix ctm;
	fz_point page_pos;

	fz_invert_matrix(&ctm, &annot->page->ctm);

	dr = pdf_dict_gets(annot->page->me, "Resources");
	if (!dr)
	{
		dr = pdf_new_dict(doc, 1);
		pdf_dict_putp_drop(annot->page->me, "Resources", dr);
	}

	/* Ensure the resource dictionary includes a font dict */
	form_fonts = pdf_dict_gets(dr, "Font");
	if (!form_fonts)
	{
		form_fonts = pdf_new_dict(doc, 1);
		pdf_dict_puts_drop(dr, "Font", form_fonts);
		/* form_fonts is still valid if execution continues past the above call */
	}

	fz_var(fzbuf);
	fz_var(font);
	fz_try(ctx)
	{
		unsigned char *da_str;
		int da_len;
		fz_rect bounds;

		find_free_font_name(form_fonts, nbuf, sizeof(nbuf));

		font = pdf_new_dict(doc, 5);
		ref = pdf_new_ref(doc, font);
		pdf_dict_puts_drop(form_fonts, nbuf, ref);

		pdf_dict_puts_drop(font, "Type", pdf_new_name(doc, "Font"));
		pdf_dict_puts_drop(font, "Subtype", pdf_new_name(doc, "Type1"));
		pdf_dict_puts_drop(font, "BaseFont", pdf_new_name(doc, font_name));
		pdf_dict_puts_drop(font, "Encoding", pdf_new_name(doc, "WinAnsiEncoding"));

		memcpy(da_info.col, color, sizeof(float)*3);
		da_info.col_size = 3;
		da_info.font_name = nbuf;
		da_info.font_size = font_size;

		fzbuf = fz_new_buffer(ctx, 0);
		pdf_fzbuf_print_da(ctx, fzbuf, &da_info);

		da_len = fz_buffer_storage(ctx, fzbuf, &da_str);
		pdf_dict_puts_drop(annot->obj, "DA", pdf_new_string(doc, (char *)da_str, da_len));

		/* FIXME: should convert to WinAnsiEncoding */
		pdf_dict_puts_drop(annot->obj, "Contents", pdf_new_string(doc, text, strlen(text)));

		font_desc = pdf_load_font(doc, NULL, font, 0);
		pdf_measure_text(ctx, font_desc, (unsigned char *)text, strlen(text), &bounds);

		page_pos = *pos;
		fz_transform_point(&page_pos, &ctm);

		bounds.x0 *= font_size;
		bounds.x1 *= font_size;
		bounds.y0 *= font_size;
		bounds.y1 *= font_size;

		bounds.x0 += page_pos.x;
		bounds.x1 += page_pos.x;
		bounds.y0 += page_pos.y;
		bounds.y1 += page_pos.y;

		pdf_dict_puts_drop(annot->obj, "Rect", pdf_new_rect(doc, &bounds));
		update_rect(ctx, annot);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(font);
		fz_drop_buffer(ctx, fzbuf);
		pdf_drop_font(ctx, font_desc);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}
