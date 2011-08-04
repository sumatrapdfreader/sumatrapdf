#include "fitz.h"
#include "mupdf.h"

void
pdf_free_link(pdf_link *link)
{
	if (link->next)
		pdf_free_link(link->next);
	if (link->dest)
		fz_drop_obj(link->dest);
	fz_free(link);
}

static fz_obj *
resolve_dest(pdf_xref *xref, fz_obj *dest)
{
	if (fz_is_name(dest) || fz_is_string(dest))
	{
		dest = pdf_lookup_dest(xref, dest);
		return resolve_dest(xref, dest);
	}

	else if (fz_is_array(dest))
	{
		return dest;
	}

	else if (fz_is_dict(dest))
	{
		dest = fz_dict_gets(dest, "D");
		return resolve_dest(xref, dest);
	}

	else if (fz_is_indirect(dest))
		return dest;

	return NULL;
}

pdf_link *
pdf_load_link(pdf_xref *xref, fz_obj *dict)
{
	fz_obj *dest;
	fz_obj *action;
	fz_obj *obj;
	fz_rect bbox;
	pdf_link_kind kind;

	dest = NULL;

	obj = fz_dict_gets(dict, "Rect");
	if (obj)
		bbox = pdf_to_rect(obj);
	else
		bbox = fz_empty_rect;

	obj = fz_dict_gets(dict, "Dest");
	if (obj)
	{
		kind = PDF_LINK_GOTO;
		dest = resolve_dest(xref, obj);
	}

	action = fz_dict_gets(dict, "A");

	/* fall back to additional action button's down/up action */
	if (!action)
		action = fz_dict_getsa(fz_dict_gets(dict, "AA"), "U", "D");

	if (action)
	{
		obj = fz_dict_gets(action, "S");
		if (fz_is_name(obj) && !strcmp(fz_to_name(obj), "GoTo"))
		{
			kind = PDF_LINK_GOTO;
			dest = resolve_dest(xref, fz_dict_gets(action, "D"));
		}
		else if (fz_is_name(obj) && !strcmp(fz_to_name(obj), "URI"))
		{
			kind = PDF_LINK_URI;
			dest = fz_dict_gets(action, "URI");
		}
		else if (fz_is_name(obj) && !strcmp(fz_to_name(obj), "Launch"))
		{
			kind = PDF_LINK_LAUNCH;
			dest = fz_dict_gets(action, "F");
		}
		else if (fz_is_name(obj) && !strcmp(fz_to_name(obj), "Named"))
		{
			kind = PDF_LINK_NAMED;
			dest = fz_dict_gets(action, "N");
		}
		else if (fz_is_name(obj) && (!strcmp(fz_to_name(obj), "GoToR")))
		{
			kind = PDF_LINK_ACTION;
			dest = action;
		}
		else
		{
			dest = NULL;
		}
	}

	if (dest)
	{
		pdf_link *link = fz_malloc(sizeof(pdf_link));
		link->kind = kind;
		link->rect = bbox;
		link->dest = fz_keep_obj(dest);
		link->next = NULL;
		return link;
	}

	return NULL;
}

void
pdf_load_links(pdf_link **linkp, pdf_xref *xref, fz_obj *annots)
{
	pdf_link *link, *head, *tail;
	fz_obj *obj;
	int i;

	head = tail = NULL;
	link = NULL;

	for (i = 0; i < fz_array_len(annots); i++)
	{
		obj = fz_array_get(annots, i);
		link = pdf_load_link(xref, obj);
		if (link)
		{
			if (!head)
				head = tail = link;
			else
			{
				tail->next = link;
				tail = link;
			}
		}
	}

	*linkp = head;
}

void
pdf_free_annot(pdf_annot *annot)
{
	if (annot->next)
		pdf_free_annot(annot->next);
	if (annot->ap)
		pdf_drop_xobject(annot->ap);
	if (annot->obj)
		fz_drop_obj(annot->obj);
	fz_free(annot);
}

static void
pdf_transform_annot(pdf_annot *annot)
{
	fz_matrix matrix = annot->ap->matrix;
	fz_rect bbox = annot->ap->bbox;
	fz_rect rect = annot->rect;
	float w, h, x, y;

	bbox = fz_transform_rect(matrix, bbox);
	w = (rect.x1 - rect.x0) / (bbox.x1 - bbox.x0);
	h = (rect.y1 - rect.y0) / (bbox.y1 - bbox.y0);
	x = rect.x0 - bbox.x0;
	y = rect.y0 - bbox.y0;
	annot->matrix = fz_concat(fz_scale(w, h), fz_translate(x, y));
}

/* SumatraPDF: partial support for link borders */
static pdf_annot *
pdf_create_annot(fz_rect rect, fz_obj *base_obj, unsigned char *content, int transparent)
{
	pdf_annot *annot;
	pdf_xobject *form;

	form = fz_malloc(sizeof(pdf_xobject));
	memset(form, 0, sizeof(pdf_xobject));
	form->refs = 1;
	form->matrix = fz_identity;
	form->bbox.x1 = rect.x1 - rect.x0;
	form->bbox.y1 = rect.y1 - rect.y0;
	form->transparency = transparent;
	form->isolated = !transparent;

	form->contents = fz_new_buffer(strlen(content));
	form->contents->len = form->contents->cap;
	memcpy(form->contents->data, content, form->contents->len);

	annot = fz_malloc(sizeof(pdf_annot));
	annot->obj = base_obj;
	annot->rect = rect;
	annot->ap = form;
	annot->next = NULL;

	pdf_transform_annot(annot);

	return annot;
}

static pdf_annot *
pdf_annot_for_link_border(pdf_xref *xref, fz_obj *obj)
{
	fz_obj *border, *color, *dashes;
	fz_rect rect;
	unsigned char *string;
	fz_stream *stream;
	int i;

	if (strcmp(fz_to_name(fz_dict_gets(obj, "Subtype")), "Link") != 0)
		return NULL;
	border = fz_dict_gets(obj, "Border");
	if (fz_to_real(fz_array_get(border, 2)) <= 0)
		return NULL;

	color = fz_dict_gets(obj, "C");
	dashes = fz_array_get(border, 3);
	rect = pdf_to_rect(fz_dict_gets(obj, "Rect"));

	// manually construct an XObject for the link border

	// display the borders only for the View target
	string = "<< /OC << /OCGs << /Usage << /Print << /PrintState /OFF >> /Export << /ExportState /OFF >> >> >> >> >>";
	stream = fz_open_memory(string, strlen(string));
	pdf_parse_stm_obj(&obj, NULL, stream, xref->scratch, sizeof(xref->scratch));
	fz_close(stream);

	// TODO: draw rounded rectangles if the first two /Border values are non-zero
	sprintf(xref->scratch, "q %.4f w [", fz_to_real(fz_array_get(border, 2)));
	string = xref->scratch + strlen(xref->scratch);
	for (i = 0; i < fz_array_len(dashes); i++)
	{
		sprintf(string, "%.4f ", fz_to_real(fz_array_get(dashes, i)));
		string += strlen(string);
	}
	sprintf(string, "] 0 d %.4f %.4f %.4f RG 0 0 %.4f %.4f re S Q",
		fz_to_real(fz_array_get(color, 0)), fz_to_real(fz_array_get(color, 1)),
		fz_to_real(fz_array_get(color, 2)), rect.x1 - rect.x0, rect.y1 - rect.y0);

	return pdf_create_annot(rect, obj, xref->scratch, 0);
}

void
pdf_load_annots(pdf_annot **annotp, pdf_xref *xref, fz_obj *annots)
{
	pdf_annot *annot, *head, *tail;
	fz_obj *obj, *ap, *as, *n, *rect;
	pdf_xobject *form;
	fz_error error;
	int i;

	head = tail = NULL;
	annot = NULL;

	for (i = 0; i < fz_array_len(annots); i++)
	{
		obj = fz_array_get(annots, i);

		rect = fz_dict_gets(obj, "Rect");
		ap = fz_dict_gets(obj, "AP");
		as = fz_dict_gets(obj, "AS");
		if (fz_is_dict(ap))
		{
			n = fz_dict_gets(ap, "N"); /* normal state */

			/* lookup current state in sub-dictionary */
			if (!pdf_is_stream(xref, fz_to_num(n), fz_to_gen(n)))
				n = fz_dict_get(n, as);

			if (pdf_is_stream(xref, fz_to_num(n), fz_to_gen(n)))
			{
				error = pdf_load_xobject(&form, xref, n);
				if (error)
				{
					fz_catch(error, "ignoring broken annotation");
					continue;
				}

				annot = fz_malloc(sizeof(pdf_annot));
				annot->obj = fz_keep_obj(obj);
				annot->rect = pdf_to_rect(rect);
				annot->ap = form;
				annot->next = NULL;

				pdf_transform_annot(annot);

				if (annot)
				{
					if (!head)
						head = tail = annot;
					else
					{
						tail->next = annot;
						tail = annot;
					}
				}
			}
		}
		/* SumatraPDF: partial support for link borders */
		else if ((annot = pdf_annot_for_link_border(xref, obj)))
		{
			if (!head)
				head = tail = annot;
			else
			{
				tail->next = annot;
				tail = annot;
			}
		}
	}

	*annotp = head;
}
