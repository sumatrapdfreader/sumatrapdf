#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>
#include <assert.h>

static void
pdf_filter_xobject(fz_context *ctx, pdf_document *doc, pdf_obj *xobj, pdf_obj *page_res, pdf_filter_options *filter);

static void
pdf_filter_type3(fz_context *ctx, pdf_document *doc, pdf_obj *obj, pdf_obj *page_res, pdf_filter_options *filter);

static void
pdf_filter_resources(fz_context *ctx, pdf_document *doc, pdf_obj *in_res, pdf_obj *res, pdf_filter_options *filter)
{
	pdf_obj *obj;
	int i, n;

	if (!filter->recurse)
		return;

	/* ExtGState */
	obj = pdf_dict_get(ctx, res, PDF_NAME(ExtGState));
	if (obj)
	{
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
		{
			pdf_obj *smask = pdf_dict_get(ctx, pdf_dict_get_val(ctx, obj, i), PDF_NAME(SMask));
			if (smask)
			{
				pdf_obj *g = pdf_dict_get(ctx, smask, PDF_NAME(G));
				if (g)
				{
					/* Transparency group XObject */
					pdf_filter_xobject(ctx, doc, g, in_res, filter);
				}
			}
		}
	}

	/* Pattern */
	obj = pdf_dict_get(ctx, res, PDF_NAME(Pattern));
	if (obj)
	{
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
		{
			pdf_obj *pat = pdf_dict_get_val(ctx, obj, i);
			if (pat && pdf_dict_get_int(ctx, pat, PDF_NAME(PatternType)) == 1)
			{
				pdf_filter_xobject(ctx, doc, pat, in_res, filter);
			}
		}
	}

	/* XObject */
	if (!filter->instance_forms)
	{
		obj = pdf_dict_get(ctx, res, PDF_NAME(XObject));
		if (obj)
		{
			n = pdf_dict_len(ctx, obj);
			for (i = 0; i < n; i++)
			{
				pdf_obj *xobj = pdf_dict_get_val(ctx, obj, i);
				if (xobj && pdf_dict_get(ctx, xobj, PDF_NAME(Subtype)) == PDF_NAME(Form))
	{
					pdf_filter_xobject(ctx, doc, xobj, in_res, filter);
				}
			}
		}
	}

	/* Font */
	obj = pdf_dict_get(ctx, res, PDF_NAME(Font));
	if (obj)
		{
		n = pdf_dict_len(ctx, obj);
		for (i = 0; i < n; i++)
		{
			pdf_obj *font = pdf_dict_get_val(ctx, obj, i);
			if (font && pdf_dict_get(ctx, font, PDF_NAME(Subtype)) == PDF_NAME(Type3))
			{
				pdf_filter_type3(ctx, doc, font, in_res, filter);
			}
		}
		}

}

/*
	Clean a content stream's rendering operations, with an optional post
	processing step.

	Firstly, this filters the PDF operators used to avoid (some cases of)
	repetition, and leaves the content stream in a balanced state with an
	unchanged top level matrix etc. At the same time, the resources actually
	used are collected into a new resource dictionary.

	Next, the resources themselves are recursively cleaned (as appropriate)
	in the same way, if the 'recurse' flag is set.
*/
static void
pdf_filter_content_stream(
	fz_context *ctx,
	pdf_document *doc,
	pdf_obj *in_stm,
	pdf_obj *in_res,
	fz_matrix transform,
	pdf_filter_options *filter,
	int struct_parents,
	fz_buffer **out_buf,
	pdf_obj **out_res)
{
	pdf_processor *proc_buffer = NULL;
	pdf_processor *proc_filter = NULL;

	fz_var(proc_buffer);
	fz_var(proc_filter);

	*out_buf = NULL;
	*out_res = NULL;

	fz_try(ctx)
		{
		*out_buf = fz_new_buffer(ctx, 1024);
		proc_buffer = pdf_new_buffer_processor(ctx, *out_buf, filter->ascii);
		if (filter->sanitize)
		{
			*out_res = pdf_new_dict(ctx, doc, 1);
			proc_filter = pdf_new_filter_processor(ctx, doc, proc_buffer, in_res, *out_res, struct_parents, transform, filter);
			pdf_process_contents(ctx, proc_filter, doc, in_res, in_stm, NULL);
			pdf_close_processor(ctx, proc_filter);
		}
		else
		{
			*out_res = pdf_keep_obj(ctx, in_res);
			pdf_process_contents(ctx, proc_buffer, doc, in_res, in_stm, NULL);
		}
		pdf_close_processor(ctx, proc_buffer);

		pdf_filter_resources(ctx, doc, in_res, *out_res, filter);
	}
	fz_always(ctx)
	{
		pdf_drop_processor(ctx, proc_filter);
		pdf_drop_processor(ctx, proc_buffer);
	}
	fz_catch(ctx)
	{
		fz_drop_buffer(ctx, *out_buf);
		*out_buf = NULL;
		pdf_drop_obj(ctx, *out_res);
		*out_res = NULL;
		fz_rethrow(ctx);
	}
}

/*
	Clean a Type 3 font's CharProcs content streams. This works almost
	exactly like pdf_filter_content_stream, but the resource dictionary is
	shared between all off the CharProcs.
*/
static void
pdf_filter_type3(fz_context *ctx, pdf_document *doc, pdf_obj *obj, pdf_obj *page_res, pdf_filter_options *filter)
{
	pdf_processor *proc_buffer = NULL;
	pdf_processor *proc_filter = NULL;
	pdf_obj *in_res;
	pdf_obj *out_res = NULL;
	pdf_obj *charprocs;
	int i, n;

	fz_var(out_res);
	fz_var(proc_buffer);
	fz_var(proc_filter);

	/* We cannot combine instancing with type3 fonts. The new names for
	 * instanced form/image resources would clash, since they start over for
	 * each content stream. This is not a problem for now, because we only
	 * use instancing with redaction, and redaction doesn't clean type3
	 * fonts.
	 */
	assert(!filter->instance_forms);

	/* Avoid recursive cycles! */
	if (pdf_mark_obj(ctx, obj))
		return;

	fz_try(ctx)
	{
		in_res = pdf_dict_get(ctx, obj, PDF_NAME(Resources));
		if (!in_res)
			in_res = page_res;

		if (filter->sanitize)
			out_res = pdf_new_dict(ctx, doc, 1);
		else
			out_res = pdf_keep_obj(ctx, in_res);

		charprocs = pdf_dict_get(ctx, obj, PDF_NAME(CharProcs));
		n = pdf_dict_len(ctx, charprocs);
		for (i = 0; i < n; i++)
		{
			pdf_obj *val = pdf_dict_get_val(ctx, charprocs, i);
			fz_buffer *buffer = fz_new_buffer(ctx, 1024);
			fz_try(ctx)
			{
				proc_buffer = pdf_new_buffer_processor(ctx, buffer, filter->ascii);
				if (filter->sanitize)
				{
					proc_filter = pdf_new_filter_processor(ctx, doc, proc_buffer, in_res, out_res, -1, fz_identity, filter);
					pdf_process_contents(ctx, proc_filter, doc, in_res, val, NULL);
					pdf_close_processor(ctx, proc_filter);
				}
				else
				{
					pdf_process_contents(ctx, proc_buffer, doc, in_res, val, NULL);
				}
				pdf_close_processor(ctx, proc_buffer);

				pdf_update_stream(ctx, doc, val, buffer, 0);
			}
			fz_always(ctx)
			{
				pdf_drop_processor(ctx, proc_filter);
				pdf_drop_processor(ctx, proc_buffer);
				fz_drop_buffer(ctx, buffer);
			}
			fz_catch(ctx)
			{
				fz_rethrow(ctx);
			}
		}

		pdf_filter_resources(ctx, doc, in_res, out_res, filter);

		if (filter->sanitize)
			pdf_dict_put(ctx, obj, PDF_NAME(Resources), out_res);
	}
	fz_always(ctx)
	{
		pdf_unmark_obj(ctx, obj);
		pdf_drop_obj(ctx, out_res);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
pdf_filter_xobject(fz_context *ctx, pdf_document *doc, pdf_obj *stm, pdf_obj *page_res, pdf_filter_options *filter)
{
	pdf_obj *struct_parents_obj;
	int struct_parents;
	pdf_obj *new_res = NULL;
	fz_buffer *new_buf = NULL;
	pdf_obj *old_res;

	fz_var(new_buf);
	fz_var(new_res);

	// TODO for RJW: XObject can also be a StructParent; how do we handle that case?

	struct_parents_obj = pdf_dict_get(ctx, stm, PDF_NAME(StructParents));
	struct_parents = -1;
	if (pdf_is_number(ctx, struct_parents_obj))
		struct_parents = pdf_to_int(ctx, struct_parents_obj);

	old_res = pdf_dict_get(ctx, stm, PDF_NAME(Resources));
	if (!old_res)
		old_res = page_res;

	// TODO: don't clean objects more than once.

	/* Avoid recursive cycles! */
	if (pdf_mark_obj(ctx, stm))
		return;
	fz_try(ctx)
	{
		pdf_filter_content_stream(ctx, doc, stm, old_res, fz_identity, filter, struct_parents, &new_buf, &new_res);
		pdf_update_stream(ctx, doc, stm, new_buf, 0);
		pdf_dict_put(ctx, stm, PDF_NAME(Resources), new_res);
		}
	fz_always(ctx)
		{
		pdf_unmark_obj(ctx, stm);
		fz_drop_buffer(ctx, new_buf);
		pdf_drop_obj(ctx, new_res);
		}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

pdf_obj *
pdf_filter_xobject_instance(fz_context *ctx, pdf_obj *old_xobj, pdf_obj *page_res, fz_matrix transform, pdf_filter_options *filter)
{
	pdf_document *doc = pdf_get_bound_document(ctx, old_xobj);
	pdf_obj *new_xobj;
	pdf_obj *new_res, *old_res;
	fz_buffer *new_buf;
	pdf_obj *struct_parents_obj;
	int struct_parents;

	fz_var(new_xobj);
	fz_var(new_buf);
	fz_var(new_res);

	// TODO for RJW: XObject can also be a StructParent; how do we handle that case?
	// TODO for RJW: will we run into trouble by duplicating StructParents stuff?

	struct_parents_obj = pdf_dict_get(ctx, old_xobj, PDF_NAME(StructParents));
	struct_parents = -1;
	if (pdf_is_number(ctx, struct_parents_obj))
		struct_parents = pdf_to_int(ctx, struct_parents_obj);

	old_res = pdf_dict_get(ctx, old_xobj, PDF_NAME(Resources));
	if (!old_res)
		old_res = page_res;

	if (pdf_mark_obj(ctx, old_xobj))
		return pdf_keep_obj(ctx, old_xobj);

	fz_try(ctx)
		{
		new_xobj = pdf_add_object_drop(ctx, doc, pdf_copy_dict(ctx, old_xobj));
		pdf_filter_content_stream(ctx, doc, old_xobj, old_res, transform, filter, struct_parents, &new_buf, &new_res);
		pdf_update_stream(ctx, doc, new_xobj, new_buf, 0);
		pdf_dict_put(ctx, new_xobj, PDF_NAME(Resources), new_res);
		}
	fz_always(ctx)
		{
		pdf_unmark_obj(ctx, old_xobj);
		fz_drop_buffer(ctx, new_buf);
		pdf_drop_obj(ctx, new_res);
		}
	fz_catch(ctx)
		{
		pdf_drop_obj(ctx, new_xobj);
		fz_rethrow(ctx);
		}

	return new_xobj;
}

void pdf_filter_page_contents(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_filter_options *filter)
{
	pdf_obj *contents, *old_res;
	pdf_obj *struct_parents_obj;
	pdf_obj *new_res;
	fz_buffer *buffer;
	int struct_parents;

	struct_parents_obj = pdf_dict_get(ctx, page->obj, PDF_NAME(StructParents));
	struct_parents = -1;
	if (pdf_is_number(ctx, struct_parents_obj))
		struct_parents = pdf_to_int(ctx, struct_parents_obj);

	contents = pdf_page_contents(ctx, page);
	old_res = pdf_page_resources(ctx, page);

	pdf_filter_content_stream(ctx, doc, contents, old_res, fz_identity, filter, struct_parents, &buffer, &new_res);

	fz_try(ctx)
	{
		if (filter->end_page)
			filter->end_page(ctx, buffer, filter->opaque);
		if (pdf_is_array(ctx, contents))
		{
			/* Create a new stream object to replace the array of streams. */
			contents = pdf_add_object_drop(ctx, doc, pdf_new_dict(ctx, doc, 1));
			pdf_dict_put_drop(ctx, page->obj, PDF_NAME(Contents), contents);
		}
		pdf_update_stream(ctx, doc, contents, buffer, 0);
		pdf_dict_put(ctx, page->obj, PDF_NAME(Resources), new_res);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buffer);
		pdf_drop_obj(ctx, new_res);
	}
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void pdf_filter_annot_contents(fz_context *ctx, pdf_document *doc, pdf_annot *annot, pdf_filter_options *filter)
{
	pdf_obj *ap = pdf_dict_get(ctx, annot->obj, PDF_NAME(AP));
	if (pdf_is_dict(ctx, ap))
	{
		int i, n = pdf_dict_len(ctx, ap);
	for (i = 0; i < n; i++)
	{
			pdf_obj *stm = pdf_dict_get_val(ctx, ap, i);
			if (pdf_is_stream(ctx, stm))
			{
				pdf_filter_xobject(ctx, doc, stm, NULL, filter);
			}
		}
	}
}

static void
pdf_redact_end_page(fz_context *ctx, fz_buffer *buf, void *opaque)
{
	pdf_page *page = opaque;
	pdf_annot *annot;
	pdf_obj *qp;
	int i, n;

	fz_append_string(ctx, buf, "0 g\n");

	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
	{
		if (pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype)) == PDF_NAME(Redact))
		{
			qp = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
			n = pdf_array_len(ctx, qp);
			if (n > 0)
			{
				for (i = 0; i < n; i += 8)
				{
					fz_quad q = pdf_to_quad(ctx, qp, i);
					fz_append_printf(ctx, buf, "%g %g m\n", q.ll.x, q.ll.y);
					fz_append_printf(ctx, buf, "%g %g l\n", q.lr.x, q.lr.y);
					fz_append_printf(ctx, buf, "%g %g l\n", q.ur.x, q.ur.y);
					fz_append_printf(ctx, buf, "%g %g l\n", q.ul.x, q.ul.y);
					fz_append_string(ctx, buf, "f\n");
				}
			}
			else
			{
				fz_rect r = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
				fz_append_printf(ctx, buf, "%g %g m\n", r.x0, r.y0);
				fz_append_printf(ctx, buf, "%g %g l\n", r.x1, r.y0);
				fz_append_printf(ctx, buf, "%g %g l\n", r.x1, r.y1);
				fz_append_printf(ctx, buf, "%g %g l\n", r.x0, r.y1);
				fz_append_string(ctx, buf, "f\n");
			}
		}
	}
}

static int
pdf_redact_text_filter(fz_context *ctx, void *opaque, int *ucsbuf, int ucslen, fz_matrix trm, fz_matrix ctm, fz_rect bbox)
{
	pdf_page *page = opaque;
	pdf_annot *annot;
	pdf_obj *qp;
	fz_point p;
	fz_rect r;
	fz_quad q;
	int i, n;

	trm = fz_concat(trm, ctm);
	p = fz_make_point(trm.e, trm.f);

	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
	{
		if (pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype)) == PDF_NAME(Redact))
		{
			qp = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
			n = pdf_array_len(ctx, qp);
			if (n > 0)
			{
				for (i = 0; i < n; i += 8)
				{
					q = pdf_to_quad(ctx, qp, i);
					if (fz_is_point_inside_quad(p, q))
						return 1;
				}
			}
			else
			{
				r = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
				if (fz_is_point_inside_rect(p, r))
					return 1;
			}
		}
	}

	return 0;
}

static int
pdf_redact_image_filter(fz_context *ctx, void *opaque, fz_matrix ctm, const char *name, fz_image *image)
{
	pdf_page *page = opaque;
	pdf_annot *annot;
	pdf_obj *qp;
	fz_rect area;
	fz_rect r;
	fz_quad q;
	int i, n;

	area = fz_transform_rect(fz_unit_rect, ctm);

	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
	{
		if (pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype)) == PDF_NAME(Redact))
		{
			qp = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
			n = pdf_array_len(ctx, qp);
			if (n > 0)
			{
				for (i = 0; i < n; i += 8)
				{
					q = pdf_to_quad(ctx, qp, i);
					r = fz_rect_from_quad(q);
					r = fz_intersect_rect(r, area);
					if (!fz_is_empty_rect(r))
						return 1;
				}
			}
			else
			{
				r = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
				r = fz_intersect_rect(r, area);
				if (!fz_is_empty_rect(r))
					return 1;
			}
		}
	}

	return 0;
}

static int
pdf_redact_page_link(fz_context *ctx, pdf_document *doc, pdf_page *page, fz_rect area)
{
	pdf_annot *annot;
	pdf_obj *qp;
	fz_quad q;
	fz_rect r;
	int i, n;

	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
	{
		if (pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype)) == PDF_NAME(Redact))
		{
			qp = pdf_dict_get(ctx, annot->obj, PDF_NAME(QuadPoints));
			n = pdf_array_len(ctx, qp);
			if (n > 0)
			{
				for (i = 0; i < n; i += 8)
				{
					q = pdf_to_quad(ctx, qp, i);
					r = fz_rect_from_quad(q);
					r = fz_intersect_rect(r, area);
					if (!fz_is_empty_rect(r))
						return 1;
				}
			}
			else
			{
				r = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
				r = fz_intersect_rect(r, area);
				if (!fz_is_empty_rect(r))
					return 1;
			}
		}
	}
	return 0;
}

static void
pdf_redact_page_links(fz_context *ctx, pdf_document *doc, pdf_page *page)
{
	pdf_obj *annots;
	pdf_obj *link;
	fz_rect area;
	int k;

	annots = pdf_dict_get(ctx, page->obj, PDF_NAME(Annots));
	k = 0;
	while (k < pdf_array_len(ctx, annots))
	{
		link = pdf_array_get(ctx, annots, k);
		if (pdf_dict_get(ctx, link, PDF_NAME(Subtype)) == PDF_NAME(Link))
		{
			area = pdf_dict_get_rect(ctx, link, PDF_NAME(Rect));
			if (pdf_redact_page_link(ctx, doc, page, area))
			{
				pdf_array_delete(ctx, annots, k);
				continue;
			}
		}
		++k;
	}
}

int
pdf_redact_page(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_redact_options *opts)
{
	pdf_annot *annot;
	int has_redactions = 0;
	int no_black_boxes = 0;
	int keep_images = 0;

	pdf_filter_options filter;

	if (opts)
	{
		no_black_boxes = opts->no_black_boxes;
		keep_images = opts->keep_images;
	}

	memset(&filter, 0, sizeof filter);
	filter.opaque = page;
	filter.text_filter = pdf_redact_text_filter;
	if (!keep_images)
		filter.image_filter = pdf_redact_image_filter;
	if (!no_black_boxes)
		filter.end_page = pdf_redact_end_page;
	filter.recurse = 0; /* don't redact patterns, softmasks, and type3 fonts */
	filter.instance_forms = 1; /* redact xobjects with instancing */
	filter.sanitize = 1;
	filter.ascii = 1;

	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
		if (pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype)) == PDF_NAME(Redact))
			has_redactions = 1;

	if (!has_redactions)
		return 0;

	pdf_filter_page_contents(ctx, doc, page, &filter);
	pdf_redact_page_links(ctx, doc, page);

	annot = pdf_first_annot(ctx, page);
	while (annot)
	{
		if (pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype)) == PDF_NAME(Redact))
		{
			pdf_delete_annot(ctx, page, annot);
			annot = pdf_first_annot(ctx, page);
		}
		else
		{
			annot = pdf_next_annot(ctx, annot);
		}
	}

	doc->redacted = 1;

	return 1;
}
