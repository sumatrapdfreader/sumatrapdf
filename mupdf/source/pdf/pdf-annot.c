#include "mupdf/pdf.h"

static pdf_obj *
resolve_dest_rec(pdf_document *doc, pdf_obj *dest, fz_link_kind kind, int depth)
{
	if (depth > 10) /* Arbitrary to avoid infinite recursion */
		return NULL;

	if (pdf_is_name(dest) || pdf_is_string(dest))
	{
		if (kind == FZ_LINK_GOTO)
		{
			dest = pdf_lookup_dest(doc, dest);
			dest = resolve_dest_rec(doc, dest, kind, depth+1);
		}

		return dest;
	}

	else if (pdf_is_array(dest))
	{
		return dest;
	}

	else if (pdf_is_dict(dest))
	{
		dest = pdf_dict_gets(dest, "D");
		return resolve_dest_rec(doc, dest, kind, depth+1);
	}

	else if (pdf_is_indirect(dest))
		return dest;

	return NULL;
}

static pdf_obj *
resolve_dest(pdf_document *doc, pdf_obj *dest, fz_link_kind kind)
{
	return resolve_dest_rec(doc, dest, kind, 0);
}

fz_link_dest
pdf_parse_link_dest(pdf_document *doc, fz_link_kind kind, pdf_obj *dest)
{
	fz_link_dest ld;
	pdf_obj *obj;

	int l_from_2 = 0;
	int b_from_3 = 0;
	int r_from_4 = 0;
	int t_from_5 = 0;
	int t_from_3 = 0;
	int t_from_2 = 0;
	int z_from_4 = 0;

	ld.kind = kind;
	ld.ld.gotor.flags = 0;
	ld.ld.gotor.lt.x = 0;
	ld.ld.gotor.lt.y = 0;
	ld.ld.gotor.rb.x = 0;
	ld.ld.gotor.rb.y = 0;
	ld.ld.gotor.page = -1;
	ld.ld.gotor.dest = NULL;

	dest = resolve_dest(doc, dest, kind);
	if (dest == NULL)
		fz_throw(doc->ctx, FZ_ERROR_GENERIC, "Undefined link_dest");

	if (pdf_is_name(dest))
	{
		/* SumatraPDF: expose dest as UTF-8 string */
		ld.ld.gotor.dest = fz_strdup(doc->ctx, pdf_to_name(dest));
		return ld;
	}
	else if (pdf_is_string(dest))
	{
		/* SumatraPDF: expose dest as UTF-8 string */
		ld.ld.gotor.dest = pdf_to_utf8(doc, dest);
		return ld;
	}

	obj = pdf_array_get(dest, 0);
	if (pdf_is_int(obj))
		ld.ld.gotor.page = pdf_to_int(obj);
	else
	{
		fz_try(doc->ctx)
		{
			ld.ld.gotor.page = pdf_lookup_page_number(doc, obj);
		}
		fz_catch(doc->ctx)
		{
			ld.kind = FZ_LINK_NONE;
			return ld;
		}
	}

	obj = pdf_array_get(dest, 1);
	if (!pdf_is_name(obj))
		return ld;

	if (!strcmp("XYZ", pdf_to_name(obj)))
	{
		l_from_2 = t_from_3 = z_from_4 = 1;
		ld.ld.gotor.flags |= fz_link_flag_r_is_zoom;
	}
	else if ((!strcmp("Fit", pdf_to_name(obj))) || (!strcmp("FitB", pdf_to_name(obj))))
	{
		ld.ld.gotor.flags |= fz_link_flag_fit_h;
		ld.ld.gotor.flags |= fz_link_flag_fit_v;
	}
	else if ((!strcmp("FitH", pdf_to_name(obj))) || (!strcmp("FitBH", pdf_to_name(obj))))
	{
		t_from_2 = 1;
		ld.ld.gotor.flags |= fz_link_flag_fit_h;
	}
	else if ((!strcmp("FitV", pdf_to_name(obj))) || (!strcmp("FitBV", pdf_to_name(obj))))
	{
		l_from_2 = 1;
		ld.ld.gotor.flags |= fz_link_flag_fit_v;
	}
	else if (!strcmp("FitR", pdf_to_name(obj)))
	{
		l_from_2 = b_from_3 = r_from_4 = t_from_5 = 1;
		ld.ld.gotor.flags |= fz_link_flag_fit_h;
		ld.ld.gotor.flags |= fz_link_flag_fit_v;
	}

	if (l_from_2)
	{
		obj = pdf_array_get(dest, 2);
		if (pdf_is_int(obj))
		{
			ld.ld.gotor.flags |= fz_link_flag_l_valid;
			ld.ld.gotor.lt.x = pdf_to_int(obj);
		}
		else if (pdf_is_real(obj))
		{
			ld.ld.gotor.flags |= fz_link_flag_l_valid;
			ld.ld.gotor.lt.x = pdf_to_real(obj);
		}
	}
	if (b_from_3)
	{
		obj = pdf_array_get(dest, 3);
		if (pdf_is_int(obj))
		{
			ld.ld.gotor.flags |= fz_link_flag_b_valid;
			ld.ld.gotor.rb.y = pdf_to_int(obj);
		}
		else if (pdf_is_real(obj))
		{
			ld.ld.gotor.flags |= fz_link_flag_b_valid;
			ld.ld.gotor.rb.y = pdf_to_real(obj);
		}
	}
	if (r_from_4)
	{
		obj = pdf_array_get(dest, 4);
		if (pdf_is_int(obj))
		{
			ld.ld.gotor.flags |= fz_link_flag_r_valid;
			ld.ld.gotor.rb.x = pdf_to_int(obj);
		}
		else if (pdf_is_real(obj))
		{
			ld.ld.gotor.flags |= fz_link_flag_r_valid;
			ld.ld.gotor.rb.x = pdf_to_real(obj);
		}
	}
	if (t_from_5 || t_from_3 || t_from_2)
	{
		if (t_from_5)
			obj = pdf_array_get(dest, 5);
		else if (t_from_3)
			obj = pdf_array_get(dest, 3);
		else
			obj = pdf_array_get(dest, 2);
		if (pdf_is_int(obj))
		{
			ld.ld.gotor.flags |= fz_link_flag_t_valid;
			ld.ld.gotor.lt.y = pdf_to_int(obj);
		}
		else if (pdf_is_real(obj))
		{
			ld.ld.gotor.flags |= fz_link_flag_t_valid;
			ld.ld.gotor.lt.y = pdf_to_real(obj);
		}
	}
	if (z_from_4)
	{
		obj = pdf_array_get(dest, 4);
		if (pdf_is_int(obj))
		{
			ld.ld.gotor.flags |= fz_link_flag_r_valid;
			ld.ld.gotor.rb.x = pdf_to_int(obj);
		}
		else if (pdf_is_real(obj))
		{
			ld.ld.gotor.flags |= fz_link_flag_r_valid;
			ld.ld.gotor.rb.x = pdf_to_real(obj);
		}
	}

	/* Duplicate the values out for the sake of stupid clients */
	if ((ld.ld.gotor.flags & (fz_link_flag_l_valid | fz_link_flag_r_valid)) == fz_link_flag_l_valid)
		ld.ld.gotor.rb.x = ld.ld.gotor.lt.x;
	if ((ld.ld.gotor.flags & (fz_link_flag_l_valid | fz_link_flag_r_valid | fz_link_flag_r_is_zoom)) == fz_link_flag_r_valid)
		ld.ld.gotor.lt.x = ld.ld.gotor.rb.x;
	if ((ld.ld.gotor.flags & (fz_link_flag_t_valid | fz_link_flag_b_valid)) == fz_link_flag_t_valid)
		ld.ld.gotor.rb.y = ld.ld.gotor.lt.y;
	if ((ld.ld.gotor.flags & (fz_link_flag_t_valid | fz_link_flag_b_valid)) == fz_link_flag_b_valid)
		ld.ld.gotor.lt.y = ld.ld.gotor.rb.y;

	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=1686 */
	/* some producers wrongly expect "/XYZ 0 0 0" to be the same as "/XYZ null null null" */
	if ((ld.ld.gotor.flags & fz_link_flag_r_is_zoom) &&
		(ld.ld.gotor.flags & (fz_link_flag_l_valid | fz_link_flag_t_valid | fz_link_flag_r_valid)) == (fz_link_flag_l_valid | fz_link_flag_t_valid | fz_link_flag_r_valid) &&
		ld.ld.gotor.lt.x == 0 && ld.ld.gotor.lt.y == 0 && ld.ld.gotor.rb.x == 0)
		ld.ld.gotor.flags = fz_link_flag_r_is_zoom;

	return ld;
}

/* SumatraPDF: parse full file specifications */
char *
pdf_file_spec_to_str(pdf_document *doc, pdf_obj *file_spec)
{
	pdf_obj *obj = NULL;
	char *path = NULL, *c;

	if (pdf_is_string(file_spec))
		obj = file_spec;
	else if (pdf_is_dict(file_spec))
	{
#ifdef _WIN32
		obj = pdf_dict_gets(file_spec, "DOS");
#else
		obj = pdf_dict_gets(file_spec, "Unix");
#endif
		if (!obj)
			obj = pdf_dict_getsa(file_spec, "UF", "F");
	}
	if (!pdf_is_string(obj))
		return NULL;

	path = pdf_to_utf8(doc, obj);
#ifdef _WIN32
	if (strcmp(pdf_to_name(pdf_dict_gets(file_spec, "FS")), "URL") != 0)
	{
		/* move the file name into the expected place and use the expected path separator */
		if (path[0] == '/' && (('A' <= path[1] && path[1] <= 'Z') || ('a' <= path[1] && path[1] <= 'z')) && path[2] == '/')
		{
			path[0] = path[1];
			path[1] = ':';
		}
		for (c = path; *c; c++)
		{
			if (*c == '/')
				*c = '\\';
		}
	}
#endif
	return path;
}

static char *
pdf_parse_file_spec(pdf_document *doc, pdf_obj *file_spec)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *filename;

	if (pdf_is_string(file_spec))
		return pdf_to_utf8(doc, file_spec);

	if (pdf_is_dict(file_spec)) {
		filename = pdf_dict_gets(file_spec, "UF");
		if (!filename)
			filename = pdf_dict_gets(file_spec, "F");
		if (!filename)
			filename = pdf_dict_gets(file_spec, "Unix");
		if (!filename)
			filename = pdf_dict_gets(file_spec, "Mac");
		if (!filename)
			filename = pdf_dict_gets(file_spec, "DOS");

		return pdf_to_utf8(doc, filename);
	}

	fz_warn(ctx, "cannot parse file specification");
	return NULL;
}

fz_link_dest
pdf_parse_action(pdf_document *doc, pdf_obj *action)
{
	fz_link_dest ld;
	pdf_obj *obj, *dest, *file_spec;
	fz_context *ctx = doc->ctx;

	UNUSED(ctx);

	ld.kind = FZ_LINK_NONE;

	if (!action)
		return ld;

	obj = pdf_dict_gets(action, "S");
	if (!strcmp(pdf_to_name(obj), "GoTo"))
	{
		dest = pdf_dict_gets(action, "D");
		ld = pdf_parse_link_dest(doc, FZ_LINK_GOTO, dest);
	}
	else if (!strcmp(pdf_to_name(obj), "URI"))
	{
		ld.kind = FZ_LINK_URI;
		ld.ld.uri.is_map = pdf_to_bool(pdf_dict_gets(action, "IsMap"));
		ld.ld.uri.uri = pdf_to_utf8(doc, pdf_dict_gets(action, "URI"));
	}
	else if (!strcmp(pdf_to_name(obj), "Launch"))
	{
		ld.kind = FZ_LINK_LAUNCH;
		file_spec = pdf_dict_gets(action, "F");
		/* SumatraPDF: parse full file specifications */
		ld.ld.launch.file_spec = pdf_file_spec_to_str(doc, file_spec);
		ld.ld.launch.new_window = pdf_to_int(pdf_dict_gets(action, "NewWindow"));
		/* SumatraPDF: support launching embedded files */
#ifdef _WIN32
		obj = pdf_dict_getsa(pdf_dict_gets(file_spec, "EF"), "DOS", "F");
#else
		obj = pdf_dict_getsa(pdf_dict_gets(file_spec, "EF"), "Unix", "F");
#endif
		ld.ld.launch.embedded_num = pdf_to_num(obj);
		ld.ld.launch.embedded_gen = pdf_to_gen(obj);
		/* SumatraPDF: support URL /Filespec */
		ld.ld.launch.is_uri = !obj && !strcmp(pdf_to_name(pdf_dict_gets(file_spec, "FS")), "URL");
	}
	else if (!strcmp(pdf_to_name(obj), "Named"))
	{
		ld.kind = FZ_LINK_NAMED;
		ld.ld.named.named = fz_strdup(ctx, pdf_to_name(pdf_dict_gets(action, "N")));
	}
	else if (!strcmp(pdf_to_name(obj), "GoToR"))
	{
		dest = pdf_dict_gets(action, "D");
		file_spec = pdf_dict_gets(action, "F");
		ld = pdf_parse_link_dest(doc, FZ_LINK_GOTOR, dest);
		/* SumatraPDF: parse full file specifications */
		ld.ld.gotor.file_spec = pdf_file_spec_to_str(doc, file_spec);
		ld.ld.gotor.new_window = pdf_to_int(pdf_dict_gets(action, "NewWindow"));
	}
	/* cf. http://code.google.com/p/sumatrapdf/issues/detail?id=2117 */
	else if (!strcmp(pdf_to_name(obj), "JavaScript"))
	{
		/* hackily extract the first URL the JavaScript action might open */
		char *js = pdf_to_utf8(doc, pdf_dict_gets(action, "JS"));
		char *url = strstr(js, "getURL(\"");
		if (url && strchr(url + 8, '"'))
		{
			url += 8;
			*strchr(url, '"') = '\0';
			ld.kind = FZ_LINK_URI;
			ld.ld.uri.is_map = 0;
			ld.ld.uri.uri = fz_strdup(ctx, url);
		}
		fz_free(ctx, js);
	}
	return ld;
}

static fz_link *
pdf_load_link(pdf_document *doc, pdf_obj *dict, const fz_matrix *page_ctm)
{
	pdf_obj *action;
	pdf_obj *obj;
	fz_rect bbox;
	fz_context *ctx = doc->ctx;
	fz_link_dest ld;

	obj = pdf_dict_gets(dict, "Rect");
	if (obj)
		pdf_to_rect(ctx, obj, &bbox);
	else
		bbox = fz_empty_rect;

	fz_transform_rect(&bbox, page_ctm);

	obj = pdf_dict_gets(dict, "Dest");
	if (obj)
		ld = pdf_parse_link_dest(doc, FZ_LINK_GOTO, obj);
	else
	{
		action = pdf_dict_gets(dict, "A");
		/* fall back to additional action button's down/up action */
		if (!action)
			action = pdf_dict_getsa(pdf_dict_gets(dict, "AA"), "U", "D");

		ld = pdf_parse_action(doc, action);
	}
	/* support clicking on embedded Flash movies, etc. (PDF 1.7 ExtensionLevel 3) */
	if (!obj && !action && (obj = pdf_dict_getp(dict, "RichMediaContent/Configurations")) != NULL)
	{
		obj = pdf_dict_gets(pdf_array_get(obj, 0), "Instances");
		action = pdf_dict_gets(pdf_array_get(obj, 0), "Asset");
		if (action)
		{
			ld.kind = FZ_LINK_LAUNCH;
			ld.ld.launch.file_spec = pdf_file_spec_to_str(doc, action);
			ld.ld.launch.new_window = 1;
#ifdef _WIN32
			obj = pdf_dict_getsa(pdf_dict_gets(action, "EF"), "DOS", "F");
#else
			obj = pdf_dict_getsa(pdf_dict_gets(action, "EF"), "Unix", "F");
#endif
			ld.ld.launch.embedded_num = pdf_to_num(obj);
			ld.ld.launch.embedded_gen = pdf_to_gen(obj);
			ld.ld.launch.is_uri = !obj && !strcmp(pdf_to_name(pdf_dict_gets(action, "FS")), "URL");
		}
	}
	if (ld.kind == FZ_LINK_NONE)
		return NULL;
	return fz_new_link(ctx, &bbox, ld);
}

fz_link *
pdf_load_link_annots(pdf_document *doc, pdf_obj *annots, const fz_matrix *page_ctm)
{
	fz_link *link, *head, *tail;
	pdf_obj *obj;
	int i, n;

	head = tail = NULL;
	link = NULL;

	n = pdf_array_len(annots);
	for (i = 0; i < n; i++)
	{
		/* FIXME: Move the try/catch out of the loop for performance? */
		fz_try(doc->ctx)
		{
			obj = pdf_array_get(annots, i);
			link = pdf_load_link(doc, obj, page_ctm);
		}
		fz_catch(doc->ctx)
		{
			fz_rethrow_if(doc->ctx, FZ_ERROR_TRYLATER);
			link = NULL;
		}

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

	return head;
}

void
pdf_free_annot(fz_context *ctx, pdf_annot *annot)
{
	pdf_annot *next;

	while (annot)
	{
		next = annot->next;
		if (annot->ap)
			pdf_drop_xobject(ctx, annot->ap);
		pdf_drop_obj(annot->obj);
		fz_free(ctx, annot);
		annot = next;
	}
}

void
pdf_transform_annot(pdf_annot *annot)
{
	fz_rect bbox = annot->ap->bbox;
	fz_rect rect = annot->rect;
	float w, h, x, y;

	fz_transform_rect(&bbox, &annot->ap->matrix);
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

	fz_pre_scale(fz_translate(&annot->matrix, x, y), w, h);
}

fz_annot_type pdf_annot_obj_type(pdf_obj *obj)
{
	char *subtype = pdf_to_name(pdf_dict_gets(obj, "Subtype"));
	if (!strcmp(subtype, "Text"))
		return FZ_ANNOT_TEXT;
	else if (!strcmp(subtype, "Link"))
		return FZ_ANNOT_LINK;
	else if (!strcmp(subtype, "FreeText"))
		return FZ_ANNOT_FREETEXT;
	else if (!strcmp(subtype, "Line"))
		return FZ_ANNOT_LINE;
	else if (!strcmp(subtype, "Square"))
		return FZ_ANNOT_SQUARE;
	else if (!strcmp(subtype, "Circle"))
		return FZ_ANNOT_CIRCLE;
	else if (!strcmp(subtype, "Polygon"))
		return FZ_ANNOT_POLYGON;
	else if (!strcmp(subtype, "PolyLine"))
		return FZ_ANNOT_POLYLINE;
	else if (!strcmp(subtype, "Highlight"))
		return FZ_ANNOT_HIGHLIGHT;
	else if (!strcmp(subtype, "Underline"))
		return FZ_ANNOT_UNDERLINE;
	else if (!strcmp(subtype, "Squiggly"))
		return FZ_ANNOT_SQUIGGLY;
	else if (!strcmp(subtype, "StrikeOut"))
		return FZ_ANNOT_STRIKEOUT;
	else if (!strcmp(subtype, "Stamp"))
		return FZ_ANNOT_STAMP;
	else if (!strcmp(subtype, "Caret"))
		return FZ_ANNOT_CARET;
	else if (!strcmp(subtype, "Ink"))
		return FZ_ANNOT_INK;
	else if (!strcmp(subtype, "Popup"))
		return FZ_ANNOT_POPUP;
	else if (!strcmp(subtype, "FileAttachment"))
		return FZ_ANNOT_FILEATTACHMENT;
	else if (!strcmp(subtype, "Sound"))
		return FZ_ANNOT_SOUND;
	else if (!strcmp(subtype, "Movie"))
		return FZ_ANNOT_MOVIE;
	else if (!strcmp(subtype, "Widget"))
		return FZ_ANNOT_WIDGET;
	else if (!strcmp(subtype, "Screen"))
		return FZ_ANNOT_SCREEN;
	else if (!strcmp(subtype, "PrinterMark"))
		return FZ_ANNOT_PRINTERMARK;
	else if (!strcmp(subtype, "TrapNet"))
		return FZ_ANNOT_TRAPNET;
	else if (!strcmp(subtype, "Watermark"))
		return FZ_ANNOT_WATERMARK;
	else if (!strcmp(subtype, "3D"))
		return FZ_ANNOT_3D;
	else
		return -1;
}

/* SumatraPDF: synthesize appearance streams for a few more annotations */
/* TODO: reuse code from pdf_form.c where possible and reasonable */

static pdf_annot *
pdf_create_annot_ex(pdf_document *doc, const fz_rect *rect, pdf_obj *base_obj, fz_buffer *content, pdf_obj *resources, int transparency, fz_annot_type type)
{
	fz_context *ctx = doc->ctx;
	pdf_xobject *form = NULL;
	pdf_annot *annot;
	int rotate;
	int num;

	fz_var(form);

	fz_try(ctx)
	{
		rotate = pdf_to_int(pdf_dict_getp(base_obj, "MK/R"));

		form = pdf_create_xobject(ctx, base_obj);
		fz_rotate(&form->matrix, rotate);
		form->bbox.x1 = ((rotate % 180) != 90) ? rect->x1 - rect->x0 : rect->y1 - rect->y0;
		form->bbox.y1 = ((rotate % 180) != 90) ? rect->y1 - rect->y0 : rect->x1 - rect->x0;
		form->transparency = transparency;
		form->isolated = !transparency;
		form->resources = resources;

		num = pdf_create_object(doc);
		pdf_update_object(doc, num, base_obj);
		pdf_update_stream(doc, num, content);
		form->contents = pdf_new_indirect(doc, num, 0);

		annot = fz_malloc_struct(ctx, pdf_annot);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, content);
	}
	fz_catch(ctx)
	{
		pdf_drop_xobject(ctx, form);
		pdf_drop_obj(base_obj);
		fz_rethrow(ctx);
	}

	annot->obj = base_obj;
	annot->rect = *rect;
	annot->ap = form;
	annot->next = NULL;
	annot->annot_type = type;
	annot->widget_type = PDF_WIDGET_TYPE_NOT_WIDGET;

	pdf_transform_annot(annot);

	return annot;
}

#define ANNOT_OC_VIEW_ONLY \
	"<< /OCGs << /Usage << /Print << /PrintState /OFF >> /Export << /ExportState /OFF >> >> >> >>"

static pdf_obj *
pdf_clone_for_view_only(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	obj = pdf_copy_dict(obj);

	fz_try(ctx)
	{
		pdf_dict_puts_drop(obj, "OC", pdf_new_obj_from_str(doc, ANNOT_OC_VIEW_ONLY));
	}
	fz_catch(ctx)
	{
		fz_warn(ctx, "annotation might be printed unexpectedly");
	}

	return obj;
}

static void
pdf_get_annot_color(pdf_obj *obj, float rgb[3])
{
	int k;
	obj = pdf_dict_gets(obj, "C");
	for (k = 0; k < 3; k++)
		rgb[k] = pdf_to_real(pdf_array_get(obj, k));
}

/* SumatraPDF: partial support for link borders */
static pdf_annot *
pdf_create_link_annot(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *border, *dashes;
	float border_width;
	fz_buffer *content = NULL;
	fz_rect rect;
	float rgb[3];
	int i, n;

	fz_var(content);

	border = pdf_dict_gets(obj, "Border");
	border_width = pdf_to_real(pdf_array_get(border, 2));
	dashes = pdf_array_get(border, 3);

	/* Adobe Reader omits the border if dashes isn't an array */
	if (border_width <= 0 || dashes && !pdf_is_array(dashes))
	{
		if (border && (border_width || dashes))
			fz_warn(ctx, "ignoring invalid link /Border array");
		return NULL;
	}

	pdf_get_annot_color(obj, rgb);
	pdf_to_rect(ctx, pdf_dict_gets(obj, "Rect"), &rect);

	fz_try(ctx)
	{
		content = fz_new_buffer(ctx, 128);

		// TODO: draw rounded rectangles if the first two /Border values are non-zero
		fz_buffer_printf(ctx, content, "q %f w [", border_width);
		for (i = 0, n = pdf_array_len(dashes); i < n; i++)
			fz_buffer_printf(ctx, content, "%f ", pdf_to_real(pdf_array_get(dashes, i)));
		fz_buffer_printf(ctx, content, "] 0 d %f %f %f RG 0 0 %f %f re S Q",
			rgb[0], rgb[1], rgb[2], rect.x1 - rect.x0, rect.y1 - rect.y0);

		obj = pdf_clone_for_view_only(doc, obj);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, content);
		fz_rethrow(ctx);
	}

	return pdf_create_annot_ex(doc, &rect, obj, content, NULL, 0, FZ_ANNOT_LINK);
}

// appearance streams adapted from Poppler's Annot.cc, licensed under GPLv2 and later
#define ANNOT_TEXT_AP_NOTE \
	"%f %f %f RG 1 J 1 j [] 0 d 4 M\n"                                    \
	"2 w 9 18 m 4 18 l 4 7 4 4 6 3 c 20 3 l 18 4 18 7 18 18 c 17 18 l S\n"      \
	"1.5 w 10 16 m 14 21 l S\n"                                                 \
	"1.85625 w\n"                                                               \
	"15.07 20.523 m 15.07 19.672 14.379 18.977 13.523 18.977 c 12.672 18.977\n" \
	"11.977 19.672 11.977 20.523 c 11.977 21.379 12.672 22.07 13.523 22.07 c\n" \
	"14.379 22.07 15.07 21.379 15.07 20.523 c h S\n"                            \
	"1 w 6.5 13.5 m 15.5 13.5 l S 6.5 10.5 m 13.5 10.5 l S\n"                   \
	"6.801 7.5 m 15.5 7.5 l S\n"

#define ANNOT_TEXT_AP_COMMENT \
	"%f %f %f RG 0 J 1 j [] 0 d 4 M 2 w\n"                                \
	"8 20 m 16 20 l 18.363 20 20 18.215 20 16 c 20 13 l 20 10.785 18.363 9\n"   \
	"16 9 c 13 9 l 8 3 l 8 9 l 8 9 l 5.637 9 4 10.785 4 13 c 4 16 l\n"          \
	"4 18.215 5.637 20 8 20 c h S\n"

#define ANNOT_TEXT_AP_KEY \
	"%f %f %f RG 0 J 1 j [] 0 d 4 M\n"                                    \
	"2 w 11.895 18.754 m 13.926 20.625 17.09 20.496 18.961 18.465 c 20.832\n"   \
	"16.434 20.699 13.27 18.668 11.398 c 17.164 10.016 15.043 9.746 13.281\n"   \
	"10.516 c 12.473 9.324 l 11.281 10.078 l 9.547 8.664 l 9.008 6.496 l\n"     \
	"7.059 6.059 l 6.34 4.121 l 5.543 3.668 l 3.375 4.207 l 2.938 6.156 l\n"    \
	"10.57 13.457 l 9.949 15.277 10.391 17.367 11.895 18.754 c h S\n"           \
	"1.5 w 16.059 15.586 m 16.523 15.078 17.316 15.043 17.824 15.512 c\n"       \
	"18.332 15.98 18.363 16.77 17.895 17.277 c 17.43 17.785 16.637 17.816\n"    \
	"16.129 17.352 c 15.621 16.883 15.59 16.094 16.059 15.586 c h S\n"

#define ANNOT_TEXT_AP_HELP \
	"%f %f %f RG 0 J 1 j [] 0 d 4 M 2.5 w\n"                              \
	"8.289 16.488 m 8.824 17.828 10.043 18.773 11.473 18.965 c 12.902 19.156\n" \
	"14.328 18.559 15.195 17.406 c 16.062 16.254 16.242 14.723 15.664 13.398\n" \
	"c S 12 8 m 12 12 16 11 16 15 c S\n"                                        \
	"q 1 0 0 -1 0 24 cm 1.539286 w\n"                                           \
	"12.684 20.891 m 12.473 21.258 12.004 21.395 11.629 21.196 c 11.254\n"      \
	"20.992 11.105 20.531 11.297 20.149 c 11.488 19.77 11.945 19.61 12.332\n"   \
	"19.789 c 12.719 19.969 12.891 20.426 12.719 20.817 c S Q\n"

#define ANNOT_TEXT_AP_PARAGRAPH \
	"%f %f %f RG 1 J 1 j [] 0 d 4 M 2 w\n"                                \
	"15 3 m 15 18 l 11 18 l 11 3 l S\n"                                         \
	"q 1 0 0 -1 0 24 cm 4 w\n"                                                  \
	"9.777 10.988 m 8.746 10.871 7.973 9.988 8 8.949 c 8.027 7.91 8.844\n"      \
	"7.066 9.879 7.004 c S Q\n"

#define ANNOT_TEXT_AP_NEW_PARAGRAPH \
	"%f %f %f RG 0 J 1 j [] 0 d 4 M 4 w\n"                                \
	"q 1 0 0 -1 0 24 cm\n"                                                      \
	"9.211 11.988 m 8.449 12.07 7.711 11.707 7.305 11.059 c 6.898 10.41\n"      \
	"6.898 9.59 7.305 8.941 c 7.711 8.293 8.449 7.93 9.211 8.012 c S Q\n"       \
	"q 1 0 0 -1 0 24 cm 1.004413 w\n"                                           \
	"18.07 11.511 m 15.113 10.014 l 12.199 11.602 l 12.711 8.323 l 10.301\n"    \
	"6.045 l 13.574 5.517 l 14.996 2.522 l 16.512 5.474 l 19.801 5.899 l\n"     \
	"17.461 8.252 l 18.07 11.511 l h S Q\n"                                     \
	"2 w 11 17 m 10 17 l 10 3 l S 14 3 m 14 13 l S\n"

#define ANNOT_TEXT_AP_INSERT \
	"%f %f %f RG 1 J 0 j [] 0 d 4 M 2 w\n"                                \
	"12 18.012 m 20 18 l S 9 10 m 17 10 l S 12 14.012 m 20 14 l S\n"            \
	"12 6.012 m 20 6.012 l S 4 12 m 6 10 l 4 8 l S 4 12 m 4 8 l S\n"

#define ANNOT_TEXT_AP_CROSS \
	"%f %f %f RG 1 J 0 j [] 0 d 4 M 2.5 w\n"                              \
	"18 5 m 6 17 l S 6 5 m 18 17 l S\n"

#define ANNOT_TEXT_AP_CIRCLE \
	"%f %f %f RG 1 J 1 j [] 0 d 4 M 2.5 w\n"                              \
	"19.5 11.5 m 19.5 7.359 16.141 4 12 4 c 7.859 4 4.5 7.359 4.5 11.5 c 4.5\n" \
	"15.641 7.859 19 12 19 c 16.141 19 19.5 15.641 19.5 11.5 c h S\n"

/* SumatraPDF: partial support for text icons */
static pdf_annot *
pdf_create_text_annot(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	fz_buffer *content = NULL;
	fz_rect rect;
	char *icon_name, *content_ap;
	float rgb[3];

	fz_var(content);

	icon_name = pdf_to_name(pdf_dict_gets(obj, "Name"));
	pdf_to_rect(ctx, pdf_dict_gets(obj, "Rect"), &rect);
	rect.x1 = rect.x0 + 24;
	rect.y0 = rect.y1 - 24;
	pdf_get_annot_color(obj, rgb);

	if (!strcmp(icon_name, "Comment"))
		content_ap = ANNOT_TEXT_AP_COMMENT;
	else if (!strcmp(icon_name, "Key"))
		content_ap = ANNOT_TEXT_AP_KEY;
	else if (!strcmp(icon_name, "Help"))
		content_ap = ANNOT_TEXT_AP_HELP;
	else if (!strcmp(icon_name, "Paragraph"))
		content_ap = ANNOT_TEXT_AP_PARAGRAPH;
	else if (!strcmp(icon_name, "NewParagraph"))
		content_ap = ANNOT_TEXT_AP_NEW_PARAGRAPH;
	else if (!strcmp(icon_name, "Insert"))
		content_ap = ANNOT_TEXT_AP_INSERT;
	else if (!strcmp(icon_name, "Cross"))
		content_ap = ANNOT_TEXT_AP_CROSS;
	else if (!strcmp(icon_name, "Circle"))
		content_ap = ANNOT_TEXT_AP_CIRCLE;
	else
		content_ap = ANNOT_TEXT_AP_NOTE;

	fz_try(ctx)
	{
		content = fz_new_buffer(ctx, 512);

		// TODO: make icons semi-transparent (cf. pdf_create_highlight_annot)?
		fz_buffer_printf(ctx, content, "q ");
		fz_buffer_printf(ctx, content, content_ap, 0.5, 0.5, 0.5);
		fz_buffer_printf(ctx, content, " 1 0 0 1 0 1 cm ");
		fz_buffer_printf(ctx, content, content_ap, rgb[0], rgb[1], rgb[2]);
		fz_buffer_printf(ctx, content, " Q", content_ap);

		obj = pdf_clone_for_view_only(doc, obj);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, content);
		fz_rethrow(ctx);
	}

	return pdf_create_annot_ex(doc, &rect, obj, content, NULL, 0, FZ_ANNOT_TEXT);
}

// appearance streams adapted from Poppler's Annot.cc, licensed under GPLv2 and later
#define ANNOT_FILE_ATTACHMENT_AP_PUSHPIN \
	"%f %f %f RG 1 J 1 j [] 0 d 4 M\n"                                    \
	"2 w 5 4 m 6 5 l S\n"                                                       \
	"11 14 m 9 12 l 6 12 l 13 5 l 13 8 l 15 10 l 18 11 l 20 11 l 12 19 l 12\n"  \
	"17 l 11 14 l h\n"                                                          \
	"3 w 6 5 m 9 8 l S\n"

#define ANNOT_FILE_ATTACHMENT_AP_PAPERCLIP \
	"%f %f %f RG 1 J 1 j [] 0 d 4 M 2 w\n"                                \
	"16.645 12.035 m 12.418 7.707 l 10.902 6.559 6.402 11.203 8.09 12.562 c\n"  \
	"14.133 18.578 l 14.949 19.387 16.867 19.184 17.539 18.465 c 20.551\n"      \
	"15.23 l 21.191 14.66 21.336 12.887 20.426 12.102 c 13.18 4.824 l 12.18\n"  \
	"3.82 6.25 2.566 4.324 4.461 c 3 6.395 3.383 11.438 4.711 12.801 c 9.648\n" \
	"17.887 l S\n"

#define ANNOT_FILE_ATTACHMENT_AP_GRAPH \
	"%f %f %f RG 1 J 1 j [] 0 d 4 M\n"                                    \
	"1 w 18.5 15.5 m 18.5 13.086 l 16.086 15.5 l 18.5 15.5 l h\n"               \
	"7 7 m 10 11 l 13 9 l 18 15 l S\n"                                          \
	"2 w 3 19 m 3 3 l 21 3 l S\n"

#define ANNOT_FILE_ATTACHMENT_AP_TAG \
	"%f %f %f RG 1 J 1 j [] 0 d 4 M\n"                                    \
	"1 w q 1 0 0 -1 0 24 cm\n"                                                  \
	"8.492 8.707 m 8.492 9.535 7.82 10.207 6.992 10.207 c 6.164 10.207 5.492\n" \
	"9.535 5.492 8.707 c 5.492 7.879 6.164 7.207 6.992 7.207 c 7.82 7.207\n"    \
	"8.492 7.879 8.492 8.707 c h S Q\n"                                         \
	"2 w\n"                                                                     \
	"2 w 20.078 11.414 m 20.891 10.602 20.785 9.293 20.078 8.586 c 14.422\n"    \
	"2.93 l 13.715 2.223 12.301 2.223 11.594 2.93 c 3.816 10.707 l 3.109\n"     \
	"11.414 2.402 17.781 3.816 19.195 c 5.23 20.609 11.594 19.902 12.301\n"     \
	"19.195 c 20.078 11.414 l h S\n"                                            \
	"1 w 11.949 13.184 m 16.191 8.941 l S 14.07 6.82 m 9.828 11.062 l S\n"      \
	"6.93 15.141 m 8 20 14.27 20.5 16 20.5 c 18.094 20.504 19.5 20 19.5 18 c\n" \
	"19.5 16.699 20.91 16.418 22.5 16.5 c S\n"

/* SumatraPDF: partial support for file attachment icons */
static pdf_annot *
pdf_create_file_annot(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	fz_buffer *content = NULL;
	fz_rect rect;
	char *icon_name, *content_ap;
	float rgb[3];

	fz_var(content);

	pdf_to_rect(ctx, pdf_dict_gets(obj, "Rect"), &rect);
	icon_name = pdf_to_name(pdf_dict_gets(obj, "Name"));
	pdf_get_annot_color(obj, rgb);

	if (!strcmp(icon_name, "Graph"))
		content_ap = ANNOT_FILE_ATTACHMENT_AP_GRAPH;
	else if (!strcmp(icon_name, "Paperclip"))
		content_ap = ANNOT_FILE_ATTACHMENT_AP_PAPERCLIP;
	else if (!strcmp(icon_name, "Tag"))
		content_ap = ANNOT_FILE_ATTACHMENT_AP_TAG;
	else
		content_ap = ANNOT_FILE_ATTACHMENT_AP_PUSHPIN;

	fz_try(ctx)
	{
		content = fz_new_buffer(ctx, 512);

		fz_buffer_printf(ctx, content, "q %f 0 0 %f 0 0 cm ",
			(rect.x1 - rect.x0) / 24, (rect.y1 - rect.y0) / 24);
		fz_buffer_printf(ctx, content, content_ap, 0.5, 0.5, 0.5);
		fz_buffer_printf(ctx, content, " 1 0 0 1 0 1 cm ");
		fz_buffer_printf(ctx, content, content_ap, rgb[0], rgb[1], rgb[2]);
		fz_buffer_printf(ctx, content, " Q", content_ap);

		obj = pdf_clone_for_view_only(doc, obj);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, content);
		fz_rethrow(ctx);
	}

	return pdf_create_annot_ex(doc, &rect, obj, content, NULL, 0, FZ_ANNOT_FILEATTACHMENT);
}

/* SumatraPDF: partial support for text markup annotations */

/* a: top/left to bottom/right; b: bottom/left to top/right */
static void
pdf_get_quadrilaterals(pdf_obj *quad_points, int i, fz_rect *a, fz_rect *b)
{
	a->x0 = pdf_to_real(pdf_array_get(quad_points, i * 8 + 0));
	a->y0 = pdf_to_real(pdf_array_get(quad_points, i * 8 + 1));
	b->x1 = pdf_to_real(pdf_array_get(quad_points, i * 8 + 2));
	b->y1 = pdf_to_real(pdf_array_get(quad_points, i * 8 + 3));
	b->x0 = pdf_to_real(pdf_array_get(quad_points, i * 8 + 4));
	b->y0 = pdf_to_real(pdf_array_get(quad_points, i * 8 + 5));
	a->x1 = pdf_to_real(pdf_array_get(quad_points, i * 8 + 6));
	a->y1 = pdf_to_real(pdf_array_get(quad_points, i * 8 + 7));
}

static fz_rect
fz_straighten_rect(fz_rect rect)
{
	fz_rect r;
	r.x0 = fz_min(rect.x0, rect.x1);
	r.y0 = fz_min(rect.y0, rect.y1);
	r.x1 = fz_max(rect.x0, rect.x1);
	r.y1 = fz_max(rect.y0, rect.y1);
	return r;
}

#define ANNOT_HIGHLIGHT_AP_RESOURCES \
	"<< /ExtGState << /GS << /Type /ExtGState /ca 0.8 /AIS false /BM /Multiply >> >> >>"

static pdf_annot *
pdf_create_highlight_annot(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	fz_buffer *content = NULL;
	pdf_obj *quad_points, *resources;
	fz_rect rect, a, b;
	fz_point skew;
	float rgb[3];
	int i, n;

	fz_var(content);

	pdf_to_rect(ctx, pdf_dict_gets(obj, "Rect"), &rect);
	quad_points = pdf_dict_gets(obj, "QuadPoints");
	for (i = 0, n = pdf_array_len(quad_points) / 8; i < n; i++)
	{
		pdf_get_quadrilaterals(quad_points, i, &a, &b);
		skew.x = 0.15 * fabs(a.y0 - b.y0);
		skew.y = 0.15 * fabs(a.x0 - b.x0);
		b.x0 -= skew.x; b.x1 += skew.x;
		b.y0 -= skew.y; b.y1 += skew.y;
		a = fz_straighten_rect(a); b = fz_straighten_rect(b);
		fz_union_rect(fz_union_rect(&rect, &a), &b);
	}
	pdf_get_annot_color(obj, rgb);

	fz_try(ctx)
	{
		content = fz_new_buffer(ctx, 512);

		fz_buffer_printf(ctx, content, "q /GS gs %f %f %f rg 1 0 0 1 -%f -%f cm ",
			rgb[0], rgb[1], rgb[2], rect.x0, rect.y0);
		for (i = 0, n = pdf_array_len(quad_points) / 8; i < n; i++)
		{
			pdf_get_quadrilaterals(quad_points, i, &a, &b);
			skew.x = 0.15 * fabs(a.y0 - b.y0);
			skew.y = 0.15 * fabs(a.x0 - b.x0);
			fz_buffer_printf(ctx, content, "%f %f m %f %f l %f %f l %f %f l h ",
				a.x0, a.y0, b.x1 + skew.x, b.y1 + skew.y, a.x1, a.y1, b.x0 - skew.x, b.y0 - skew.y);
		}
		fz_buffer_printf(ctx, content, "f Q");

		resources = pdf_new_obj_from_str(doc, ANNOT_HIGHLIGHT_AP_RESOURCES);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, content);
		fz_rethrow(ctx);
	}

	return pdf_create_annot_ex(doc, &rect, pdf_keep_obj(obj), content, resources, 1, FZ_ANNOT_HIGHLIGHT);
}

static pdf_annot *
pdf_create_markup_annot(pdf_document *doc, pdf_obj *obj, char *type)
{
	fz_context *ctx = doc->ctx;
	fz_buffer *content = NULL;
	fz_annot_type annot_type;
	pdf_obj *quad_points;
	fz_rect rect, a, b;
	float rgb[3], dot;
	fz_point offs;
	int i, n;

	fz_var(content);

	annot_type = !strcmp(type, "Underline") ? FZ_ANNOT_UNDERLINE : !strcmp(type, "StrikeOut") ? FZ_ANNOT_STRIKEOUT : FZ_ANNOT_SQUIGGLY;
	pdf_to_rect(ctx, pdf_dict_gets(obj, "Rect"), &rect);
	quad_points = pdf_dict_gets(obj, "QuadPoints");
	for (i = 0, n = pdf_array_len(quad_points) / 8; i < n; i++)
	{
		pdf_get_quadrilaterals(quad_points, i, &a, &b);
		offs.x = a.x0 - b.x0; offs.y = a.y0 - b.y0;
		dot = offs.x == 0 ? fabs(offs.y) : offs.y == 0 ? fabs(offs.x) : hypotf(offs.x, offs.y);
		offs.x = offs.x / dot / 4; offs.y = offs.y / dot / 4;
		b.x0 -= offs.x; a.x1 -= offs.x;
		b.y0 -= offs.y; a.y1 -= offs.y;
		a = fz_straighten_rect(a); b = fz_straighten_rect(b);
		fz_union_rect(fz_union_rect(&rect, &a), &b);
	}
	pdf_get_annot_color(obj, rgb);

	fz_try(ctx)
	{
		content = fz_new_buffer(ctx, 512);

		fz_buffer_printf(ctx, content, "q %f %f %f RG 1 0 0 1 -%f -%f cm 0.5 w ",
			rgb[0], rgb[1], rgb[2], rect.x0, rect.y0);
		if (annot_type == FZ_ANNOT_SQUIGGLY)
			fz_buffer_printf(ctx, content, "[1] 1.5 d ");
		for (i = 0, n = pdf_array_len(quad_points) / 8; i < n; i++)
		{
			pdf_get_quadrilaterals(quad_points, i, &a, &b);
			if (annot_type == FZ_ANNOT_STRIKEOUT)
				fz_buffer_printf(ctx, content, "%f %f m %f %f l ",
					(a.x0 + b.x0) / 2, (a.y0 + b.y0) / 2, (a.x1 + b.x1) / 2, (a.y1 + b.y1) / 2);
			else
				fz_buffer_printf(ctx, content, "%f %f m %f %f l ", b.x0, b.y0, a.x1, a.y1);
			if (annot_type == FZ_ANNOT_SQUIGGLY)
			{
				offs.x = a.x0 - b.x0; offs.y = a.y0 - b.y0;
				dot = offs.x == 0 ? fabs(offs.y) : offs.y == 0 ? fabs(offs.x) : hypotf(offs.x, offs.y);
				offs.x = offs.x / dot / 2; offs.y = offs.y / dot / 2;
				fz_buffer_printf(ctx, content, "S [1] 0.5 d %f %f m %f %f l ", b.x0 + offs.x, b.y0 + offs.y, a.x1 + offs.x, a.y1 + offs.y);
			}
		}
		fz_buffer_printf(ctx, content, "S Q");
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, content);
		fz_rethrow(ctx);
	}

	return pdf_create_annot_ex(doc, &rect, pdf_keep_obj(obj), content, NULL, 0, annot_type);
}

/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=692078 */
static pdf_obj *
pdf_dict_get_inheritable(pdf_document *doc, pdf_obj *obj, char *key)
{
	while (obj)
	{
		pdf_obj *val = pdf_dict_gets(obj, key);
		if (val)
			return val;
		obj = pdf_dict_gets(obj, "Parent");
	}
	return pdf_dict_gets(pdf_dict_gets(pdf_dict_gets(pdf_trailer(doc), "Root"), "AcroForm"), key);
}

static float
pdf_extract_font_size(pdf_document *doc, char *appearance, char **font_name)
{
	fz_context *ctx = doc->ctx;
	fz_stream *stream = fz_open_memory(ctx, (unsigned char *)appearance, strlen(appearance));
	pdf_lexbuf *lexbuf = &doc->lexbuf.base;
	float font_size = 0;
	int tok;

	*font_name = NULL;
	do
	{
		fz_try(ctx)
		{
			tok = pdf_lex(stream, lexbuf);
		}
		fz_catch(ctx)
		{
			tok = PDF_TOK_EOF;
		}
		if (tok == PDF_TOK_EOF)
		{
			fz_free(ctx, *font_name);
			*font_name = NULL;
			break;
		}
		if (tok == PDF_TOK_NAME)
		{
			fz_free(ctx, *font_name);
			fz_try(ctx)
			{
				*font_name = fz_strdup(ctx, lexbuf->scratch);
			}
			fz_catch(ctx)
			{
				*font_name = NULL;
			}
		}
		else if (tok == PDF_TOK_REAL)
		{
			font_size = lexbuf->f;
		}
		else if (tok == PDF_TOK_INT)
		{
			font_size = lexbuf->i;
		}
	} while (tok != PDF_TOK_KEYWORD || strcmp(lexbuf->scratch, "Tf") != 0);
	fz_close(stream);
	return font_size;
}

static pdf_obj *
pdf_get_ap_stream(pdf_document *doc, pdf_obj *obj)
{
	pdf_obj *ap = pdf_dict_gets(obj, "AP");
	if (!pdf_is_dict(ap))
		return NULL;

	ap = pdf_dict_gets(ap, "N");
	if (!pdf_is_stream(doc, pdf_to_num(ap), pdf_to_gen(ap)))
		ap = pdf_dict_get(ap, pdf_dict_gets(obj, "AS"));
	if (!pdf_is_stream(doc, pdf_to_num(ap), pdf_to_gen(ap)))
		return NULL;

	return ap;
}

static void
pdf_prepend_ap_background(fz_buffer *content, pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	pdf_xobject *form = NULL;
	fz_stream *ap_stm = NULL;
	fz_buffer *ap_contents = NULL;
	int i;

	pdf_obj *ap = pdf_get_ap_stream(doc, obj);
	if (!ap)
		return;

	fz_var(form);
	fz_var(ap_stm);
	fz_var(ap_contents);

	fz_try(ctx)
	{
		form = pdf_load_xobject(doc, ap);
		ap_stm = pdf_open_contents_stream(doc, form->contents);
		ap_contents = fz_read_all(ap_stm, 0);

		for (i = 0; i < ap_contents->len - 3 && memcmp(ap_contents->data + i, "/Tx", 3) != 0; i++);
		if (i == ap_contents->len - 3)
			i = ap_contents->len;
		if (content->len + i < 0)
			i = 0;
		if (content->cap < content->len + i)
			fz_resize_buffer(ctx, content, content->len + i);
		memcpy(content->data + content->len, ap_contents->data, i);
		content->len += i;
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, ap_contents);
		fz_close(ap_stm);
		pdf_drop_xobject(ctx, form);
	}
	fz_catch(ctx)
	{
		/* fail silently */
	}
}

static void
pdf_string_to_Tj(fz_context *ctx, fz_buffer *content, unsigned short *ucs2, unsigned short *end)
{
	// TODO: might be able to use fz_buffer_printf(ctx, content, "%( Tj ", ...);
	fz_buffer_printf(ctx, content, "(");
	for (; ucs2 < end; ucs2++)
	{
		// TODO: convert to CID(?)
		if (*ucs2 < 0x20 || *ucs2 == '(' || *ucs2 == ')' || *ucs2 == '\\')
			fz_buffer_printf(ctx, content, "\\%03o", *ucs2);
		else
			fz_buffer_printf(ctx, content, "%c", *ucs2);
	}
	fz_buffer_printf(ctx, content, ") Tj ");
}

static float
pdf_get_string_width(pdf_document *doc, pdf_obj *res, fz_buffer *base, unsigned short *string, unsigned short *end)
{
	fz_context *ctx = doc->ctx;
	fz_rect rect;
	float width;
	int old_len = base->len;
	fz_device *dev = fz_new_bbox_device(ctx, &rect);

	fz_try(ctx)
	{
		pdf_string_to_Tj(ctx, base, string, end);
		fz_buffer_printf(ctx, base, "ET Q EMC");
		pdf_run_glyph(doc, res, base, dev, &fz_identity, NULL, 0);
		width = rect.x1 - rect.x0;
	}
	fz_always(ctx)
	{
		base->len = old_len;
		fz_free_device(dev);
	}
	fz_catch(ctx)
	{
		width = -1;
	}

	return width;
}

#define iswspace(c) ((c) == 32 || 9 <= (c) && (c) <= 13)

static unsigned short *
pdf_append_line(pdf_document *doc, pdf_obj *res, fz_buffer *content, fz_buffer *base_ap,
	unsigned short *ucs2, float font_size, int align, float width, int is_multiline, float *x)
{
	fz_context *ctx = doc->ctx;
	unsigned short *end, *keep;
	float w, x1 = 0;

	if (is_multiline)
	{
		end = ucs2;
		do
		{
			if (*end == '\n' || *end == '\r' && *(end + 1) != '\n')
				break;

			for (keep = end + 1; *keep && !iswspace(*keep); keep++);
			w = pdf_get_string_width(doc, res, base_ap, ucs2, keep);
			if (w <= width || end == ucs2)
				end = keep;
		} while (w <= width && *end);
	}
	else
		end = ucs2 + wcslen(ucs2);

	if (align != 0)
	{
		w = pdf_get_string_width(doc, res, base_ap, ucs2, end);
		if (w < 0)
			fz_warn(ctx, "can't change the text's alignment");
		else if (align == 1 /* centered */)
			x1 = (width - w) / 2;
		else if (align == 2 /* right-aligned */)
			x1 = width - w;
		else
			fz_warn(ctx, "ignoring unknown quadding value %d", align);
	}

	fz_buffer_printf(ctx, content, "%f %f Td ", x1 - *x, -font_size);
	pdf_string_to_Tj(ctx, content, ucs2, end);
	*x = x1;

	return end + (*end ? 1 : 0);
}

static void
pdf_append_combed_line(pdf_document *doc, pdf_obj *res, fz_buffer *content, fz_buffer *base_ap,
	unsigned short *ucs2, float font_size, float width, int max_len)
{
	float comb_width = max_len > 0 ? width / max_len : 0;
	unsigned short c[2] = { 0 };
	float x = -2.0f;
	int i;

	fz_buffer_printf(doc->ctx, content, "0 %f Td ", -font_size);
	for (i = 0; i < max_len && ucs2[i]; i++)
	{
		*c = ucs2[i];
		pdf_append_line(doc, res, content, base_ap, c, 0, 1 /* centered */, comb_width, 0, &x);
		x -= comb_width;
	}
}

static pdf_annot *
pdf_update_tx_widget_annot(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	pdf_obj *ap, *res, *value;
	fz_rect rect;
	fz_matrix ctm;
	fz_buffer *content = NULL, *base_ap = NULL;
	int flags, align, rotate, is_multiline;
	float font_size, x, y;
	char *font_name = NULL;
	unsigned short *ucs2 = NULL, *rest;
	pdf_annot *annot;

	fz_var(content);
	fz_var(base_ap);
	fz_var(font_name);
	fz_var(ucs2);

	if (strcmp(pdf_to_name(pdf_dict_gets(obj, "Subtype")), "Widget") != 0)
		return NULL;
	if (!pdf_to_bool(pdf_dict_get_inheritable(doc, NULL, "NeedAppearances")) && pdf_get_ap_stream(doc, obj))
		return NULL;
	value = pdf_dict_get_inheritable(doc, obj, "FT");
	if (strcmp(pdf_to_name(value), "Tx") != 0)
		return NULL;

	ap = pdf_dict_get_inheritable(doc, obj, "DA");
	value = pdf_dict_get_inheritable(doc, obj, "V");
	if (!ap || !value)
		return NULL;

	res = pdf_dict_get_inheritable(doc, obj, "DR");
	pdf_to_rect(ctx, pdf_dict_gets(obj, "Rect"), &rect);
	rotate = pdf_to_int(pdf_dict_gets(pdf_dict_gets(obj, "MK"), "R"));
	fz_transform_rect(&rect, fz_rotate(&ctm, rotate));

	flags = pdf_to_int(pdf_dict_gets(obj, "Ff"));
	is_multiline = (flags & (1 << 12)) != 0;
	if ((flags & (1 << 25) /* richtext */))
		fz_warn(ctx, "missing support for richtext fields");
	align = pdf_to_int(pdf_dict_gets(obj, "Q"));

	font_size = pdf_extract_font_size(doc, pdf_to_str_buf(ap), &font_name);
	if (!font_size || !font_name)
		font_size = is_multiline ? 10 /* FIXME */ : floor(rect.y1 - rect.y0 - 2);

	fz_try(ctx)
	{
		content = fz_new_buffer(ctx, 256);
		base_ap = fz_new_buffer(ctx, 256);

		pdf_prepend_ap_background(content, doc, obj);
		fz_buffer_printf(ctx, content, "/Tx BMC q 1 1 %f %f re W n BT %s ",
			rect.x1 - rect.x0 - 2.0f, rect.y1 - rect.y0 - 2.0f, pdf_to_str_buf(ap));
		fz_buffer_printf(ctx, base_ap, "/Tx BMC q BT %s ", pdf_to_str_buf(ap));
		if (font_name)
		{
			pdf_font_desc *fontdesc = NULL;
			pdf_obj *font_obj = pdf_dict_gets(pdf_dict_gets(res, "Font"), font_name);
			if (font_obj)
			{
				fz_try(ctx)
				{
					fontdesc = pdf_load_font(doc, res, font_obj, 0);
				}
				fz_catch(ctx)
				{
					fontdesc = NULL;
				}
			}
			/* TODO: try to reverse the encoding instead of replacing the font */
			if (fontdesc && fontdesc->cid_to_gid && !fontdesc->cid_to_ucs || !fontdesc && pdf_dict_gets(res, "Font"))
			{
				pdf_obj *new_font = pdf_new_obj_from_str(doc, "<< /Type /Font /BaseFont /Helvetica /Subtype /Type1 >>");
				fz_free(ctx, font_name);
				font_name = NULL;
				font_name = fz_strdup(ctx, "Default");
				pdf_dict_puts_drop(pdf_dict_gets(res, "Font"), font_name, new_font);
			}
			pdf_drop_font(ctx, fontdesc);
			fontdesc = NULL;
			fz_buffer_printf(ctx, content, "/%s %f Tf ", font_name, font_size);
			fz_buffer_printf(ctx, base_ap, "/%s %f Tf ", font_name, font_size);
			fz_free(ctx, font_name);
			font_name = NULL;
		}
		y = 0.5f * (rect.y1 - rect.y0) + 0.6f * font_size;
		if (is_multiline)
			y = rect.y1 - rect.y0 - 2;
		fz_buffer_printf(ctx, content, "1 0 0 1 2 %f Tm ", y);

		ucs2 = pdf_to_ucs2(doc, value);
		for (rest = ucs2; *rest; rest++)
			if (*rest > 0xFF)
				*rest = '?';
		if ((flags & (1 << 13) /* password */))
			for (rest = ucs2; *rest; rest++)
				*rest = '*';

		x = 0;
		rest = ucs2;
		if ((flags & (1 << 24) /* comb */))
		{
			pdf_append_combed_line(doc, res, content, base_ap, ucs2, font_size, rect.x1 - rect.x0, pdf_to_int(pdf_dict_get_inheritable(doc, obj, "MaxLen")));
			rest = L"";
		}
		while (*rest)
			rest = pdf_append_line(doc, res, content, base_ap, rest, font_size, align, rect.x1 - rect.x0 - 4.0f, is_multiline, &x);

		fz_buffer_printf(ctx, content, "ET Q EMC");
	}
	fz_always(ctx)
	{
		fz_free(ctx, ucs2);
		fz_drop_buffer(ctx, base_ap);
	}
	fz_catch(ctx)
	{
		fz_free(ctx, font_name);
		fz_drop_buffer(ctx, content);
		fz_rethrow(ctx);
	}

	fz_transform_rect(&rect, fz_rotate(&ctm, -rotate));
	annot = pdf_create_annot_ex(doc, &rect, pdf_keep_obj(obj), content, res ? pdf_keep_obj(res) : NULL, 0, FZ_ANNOT_WIDGET);
	annot->widget_type = PDF_WIDGET_TYPE_TEXT;
	return annot;
}

/* SumatraPDF: partial support for freetext annotations */

#define ANNOT_FREETEXT_AP_RESOURCES \
	"<< /Font << /Default << /Type /Font /BaseFont /Helvetica /Subtype /Type1 >> >> >>"

static pdf_annot *
pdf_create_freetext_annot(pdf_document *doc, pdf_obj *obj)
{
	fz_context *ctx = doc->ctx;
	fz_buffer *content = NULL, *base_ap = NULL;
	pdf_obj *ap = pdf_dict_get_inheritable(doc, obj, "DA");
	pdf_obj *value = pdf_dict_gets(obj, "Contents");
	int align = pdf_to_int(pdf_dict_gets(obj, "Q"));
	pdf_obj *res = pdf_new_obj_from_str(doc, ANNOT_FREETEXT_AP_RESOURCES);
	unsigned short *ucs2 = NULL, *rest;
	fz_rect rect;
	char *r;
	float x;

	char *font_name = NULL;
	float font_size = pdf_extract_font_size(doc, pdf_to_str_buf(ap), &font_name);
	pdf_to_rect(ctx, pdf_dict_gets(obj, "Rect"), &rect);

	fz_var(content);
	fz_var(base_ap);
	fz_var(ucs2);

	fz_try(ctx)
	{
		content = fz_new_buffer(ctx, 256);
		base_ap = fz_new_buffer(ctx, 256);

		if (!font_size)
			font_size = 10;
		/* TODO: what resource dictionary does this font name refer to? */
		if (font_name)
		{
			pdf_obj *font = pdf_dict_gets(res, "Font");
			pdf_dict_puts(font, font_name, pdf_dict_gets(font, "Default"));
			fz_free(ctx, font_name);
		}

		fz_buffer_printf(ctx, content, "q 1 1 %f %f re W n BT %s ",
			rect.x1 - rect.x0 - 2.0f, rect.y1 - rect.y0 - 2.0f, pdf_to_str_buf(ap));
		fz_buffer_printf(ctx, base_ap, "q BT %s ", pdf_to_str_buf(ap));
		fz_buffer_printf(ctx, content, "/Default %f Tf ", font_size);
		fz_buffer_printf(ctx, base_ap, "/Default %f Tf ", font_size);
		fz_buffer_printf(ctx, content, "1 0 0 1 2 %f Tm ", rect.y1 - rect.y0 - 2);

		/* Adobe Reader seems to consider "[1 0 0] r" and "1 0 0 rg" to mean the same(?) */
		if ((r = strchr((char *)base_ap->data, '[')) != NULL && sscanf(r, "[%f %f %f] r", &x, &x, &x) == 3)
		{
			*r = ' ';
			memcpy(strchr(r, ']'), " rg", 3);
		}
		if ((r = strchr((char *)content->data, '[')) != NULL && sscanf(r, "[%f %f %f] r", &x, &x, &x) == 3)
		{
			*r = ' ';
			memcpy(strchr(r, ']'), " rg", 3);
		}

		ucs2 = pdf_to_ucs2(doc, value);
		for (rest = ucs2; *rest; rest++)
			if (*rest > 0xFF)
				*rest = '?';

		x = 0;
		rest = ucs2;
		while (*rest)
			rest = pdf_append_line(doc, res, content, base_ap, rest, font_size, align, rect.x1 - rect.x0 - 4.0f, 1, &x);

		fz_buffer_printf(ctx, content, "ET Q");
	}
	fz_always(ctx)
	{
		fz_free(ctx, ucs2);
		fz_drop_buffer(ctx, base_ap);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return pdf_create_annot_ex(doc, &rect, pdf_keep_obj(obj), content, res, 0, FZ_ANNOT_FREETEXT);
}

static pdf_annot *
pdf_create_annot_with_appearance(pdf_document *doc, pdf_obj *obj)
{
	char *type = pdf_to_name(pdf_dict_gets(obj, "Subtype"));

	if (!strcmp(type, "Link"))
		return pdf_create_link_annot(doc, obj);
	if (!strcmp(type, "Text"))
		return pdf_create_text_annot(doc, obj);
	if (!strcmp(type, "FileAttachment"))
		return pdf_create_file_annot(doc, obj);
	/* TODO: Adobe Reader seems to sometimes ignore the appearance stream for highlights(?) */
	if (!strcmp(type, "Highlight"))
		return pdf_create_highlight_annot(doc, obj);
	if (!strcmp(type, "Underline") || !strcmp(type, "StrikeOut") || !strcmp(type, "Squiggly"))
		return pdf_create_markup_annot(doc, obj, type);
	if (!strcmp(type, "FreeText"))
		return pdf_create_freetext_annot(doc, obj);

	return NULL;
}

void
pdf_load_annots(pdf_document *doc, pdf_page *page, pdf_obj *annots)
{
	pdf_annot *annot, **itr;
	pdf_obj *obj, *ap, *as, *n, *rect;
	int i, len, keep_annot;
	fz_context *ctx = doc->ctx;

	fz_var(annot);
	fz_var(itr);
	fz_var(keep_annot);

	itr = &page->annots;

	len = pdf_array_len(annots);
	/*
	Create an initial linked list of pdf_annot structures with only the obj field
	filled in. We do this because update_appearance has the potential to change
	the annot array, so we don't want to be iterating through the array while
	that happens.
	*/
	fz_try(ctx)
	{
		for (i = 0; i < len; i++)
		{
			obj = pdf_array_get(annots, i);
			annot = fz_malloc_struct(ctx, pdf_annot);
			annot->obj = pdf_keep_obj(obj);
			annot->page = page;
			annot->next = NULL;

			*itr = annot;
			itr = &annot->next;
		}
	}
	fz_catch(ctx)
	{
		pdf_free_annot(ctx, page->annots);
		page->annots = NULL;
		fz_rethrow(ctx);
	}

	/*
	Iterate through the newly created annot linked list, using a double pointer to
	facilitate deleting broken annotations.
	*/
	itr = &page->annots;
	while (*itr)
	{
		annot = *itr;

		fz_try(ctx)
		{
			pdf_hotspot *hp = &doc->hotspot;

			/* SumatraPDF: synthesize appearance streams for a few more annotations */
			pdf_annot *synth;
			/* cf. http://bugs.ghostscript.com/show_bug.cgi?id=692078 */
			if ((synth = pdf_update_tx_widget_annot(doc, annot->obj)) != NULL ||
				!pdf_is_dict(pdf_dict_getp(annot->obj, "AP/N")) && (synth = pdf_create_annot_with_appearance(doc, annot->obj)) != NULL)
			{
				synth->page = page;
				synth->pagerect = annot->rect;
				fz_transform_rect(&synth->pagerect, &page->ctm);
				synth->next = annot->next;
				pdf_drop_obj(annot->obj);
				*annot = *synth;
				fz_free(ctx, synth);
				itr = &annot->next;
				keep_annot = 1;
				break; // out of fz_try
			}

			n = NULL;

			/* SumatraPDF: prevent regressions */
			if (doc->update_appearance && 0)
				doc->update_appearance(doc, annot);

			obj = annot->obj;
			rect = pdf_dict_gets(obj, "Rect");
			ap = pdf_dict_gets(obj, "AP");
			as = pdf_dict_gets(obj, "AS");

			/* We only collect annotations with an appearance
			 * stream into this list, so remove any that don't
			 * (such as links) and continue. */
			keep_annot = pdf_is_dict(ap);
			if (!keep_annot)
				break;

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

			pdf_to_rect(ctx, rect, &annot->rect);
			annot->pagerect = annot->rect;
			fz_transform_rect(&annot->pagerect, &page->ctm);
			annot->ap = NULL;
			annot->annot_type = pdf_annot_obj_type(obj);
			annot->widget_type = annot->annot_type == FZ_ANNOT_WIDGET ? pdf_field_type(doc, obj) : PDF_WIDGET_TYPE_NOT_WIDGET;

			if (pdf_is_stream(doc, pdf_to_num(n), pdf_to_gen(n)))
			{
				annot->ap = pdf_load_xobject(doc, n);
				pdf_transform_annot(annot);
				annot->ap_iteration = annot->ap->iteration;
			}

			if (obj == doc->focus_obj)
				doc->focus = annot;

			/* Move to next item in the linked list */
			itr = &annot->next;
		}
		fz_catch(ctx)
		{
			if (fz_caught(ctx) == FZ_ERROR_TRYLATER)
			{
				pdf_free_annot(ctx, page->annots);
				page->annots = NULL;
				fz_rethrow(ctx);
			}
			keep_annot = 0;
			fz_warn(ctx, "ignoring broken annotation");
		}
		if (!keep_annot)
		{
			/* Move to next item in the linked list, dropping this one */
			*itr = annot->next;
			annot->next = NULL; /* Required because pdf_free_annot follows the "next" chain */
			pdf_free_annot(ctx, annot);
		}
	}

	page->annot_tailp = itr;
}

pdf_annot *
pdf_first_annot(pdf_document *doc, pdf_page *page)
{
	return page ? page->annots : NULL;
}

pdf_annot *
pdf_next_annot(pdf_document *doc, pdf_annot *annot)
{
	return annot ? annot->next : NULL;
}

fz_rect *
pdf_bound_annot(pdf_document *doc, pdf_annot *annot, fz_rect *rect)
{
	if (rect == NULL)
		return NULL;

	if (annot)
		*rect = annot->pagerect;
	else
		*rect = fz_empty_rect;
	return rect;
}

fz_annot_type
pdf_annot_type(pdf_annot *annot)
{
	return annot->annot_type;
}
