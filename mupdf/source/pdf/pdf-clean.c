#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

static void
pdf_clean_stream_object(fz_context *ctx, pdf_document *doc, pdf_obj *obj, pdf_obj *orig_res, fz_cookie *cookie, int own_res,
		pdf_text_filter_fn *text_filter, pdf_after_text_object_fn *after_text, void *arg,
		int sanitize, int ascii)
{
	pdf_processor *proc_buffer = NULL;
	pdf_processor *proc_filter = NULL;
	pdf_obj *res = NULL;
	pdf_obj *ref;
	fz_buffer *buffer;

	if (!obj)
		return;

	fz_var(res);
	fz_var(proc_buffer);
	fz_var(proc_filter);

	buffer = fz_new_buffer(ctx, 1024);

	fz_try(ctx)
	{
		pdf_obj *sp = pdf_dict_get(ctx, obj, PDF_NAME(StructParents));
		int structparents = -1;
		if (pdf_is_number(ctx, sp))
			structparents = pdf_to_int(ctx, sp);

		if (own_res)
		{
			pdf_obj *r = pdf_dict_get(ctx, obj, PDF_NAME(Resources));
			if (r)
				orig_res = r;
		}

		res = pdf_new_dict(ctx, doc, 1);

		proc_buffer = pdf_new_buffer_processor(ctx, buffer, ascii);
		proc_filter = pdf_new_filter_processor_with_text_filter(ctx, doc, structparents, proc_buffer, orig_res, res, text_filter, after_text, arg);

		pdf_process_contents(ctx, proc_filter, doc, orig_res, obj, cookie);
		pdf_close_processor(ctx, proc_filter);
		pdf_close_processor(ctx, proc_buffer);

		pdf_update_stream(ctx, doc, obj, buffer, 0);

		if (own_res)
		{
			ref = pdf_add_object(ctx, doc, res);
			pdf_dict_put_drop(ctx, obj, PDF_NAME(Resources), ref);
		}
	}
	fz_always(ctx)
	{
		pdf_drop_processor(ctx, proc_filter);
		pdf_drop_processor(ctx, proc_buffer);
		fz_drop_buffer(ctx, buffer);
		pdf_drop_obj(ctx, res);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

static void
pdf_clean_type3(fz_context *ctx, pdf_document *doc, pdf_obj *obj, pdf_obj *orig_res, fz_cookie *cookie, int sanitize, int ascii)
{
	pdf_processor *proc_buffer = NULL;
	pdf_processor *proc_filter = NULL;
	pdf_obj *res = NULL;
	pdf_obj *ref;
	pdf_obj *charprocs;
	int i, l;

	fz_var(res);
	fz_var(proc_buffer);
	fz_var(proc_filter);

	fz_try(ctx)
	{
		res = pdf_dict_get(ctx, obj, PDF_NAME(Resources));
		if (res)
			orig_res = res;
		res = NULL;

		res = pdf_new_dict(ctx, doc, 1);

		charprocs = pdf_dict_get(ctx, obj, PDF_NAME(CharProcs));
		l = pdf_dict_len(ctx, charprocs);

		for (i = 0; i < l; i++)
		{
			pdf_obj *val = pdf_dict_get_val(ctx, charprocs, i);
			fz_buffer *buffer = fz_new_buffer(ctx, 1024);
			fz_try(ctx)
			{
				proc_buffer = pdf_new_buffer_processor(ctx, buffer, ascii);
				if (sanitize)
				{
					proc_filter = pdf_new_filter_processor(ctx, doc, proc_buffer, orig_res, res);
					pdf_process_contents(ctx, proc_filter, doc, orig_res, val, cookie);
					pdf_close_processor(ctx, proc_filter);
				}
				else
				{
					pdf_process_contents(ctx, proc_filter, doc, orig_res, val, cookie);
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

		/* ProcSet - no cleaning possible. Inherit this from the old dict. */
		pdf_dict_put(ctx, res, PDF_NAME(ProcSet), pdf_dict_get(ctx, orig_res, PDF_NAME(ProcSet)));

		ref = pdf_add_object(ctx, doc, res);
		pdf_dict_put_drop(ctx, obj, PDF_NAME(Resources), ref);
	}
	fz_always(ctx)
	{
		pdf_drop_obj(ctx, res);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/*
	Clean a loaded pages rendering operations,
	with an optional post processing step.

	Firstly, this filters the PDF operators used to avoid (some cases
	of) repetition, and leaves the page in a balanced state with an
	unchanged top level matrix etc. At the same time, the resources
	used by the page contents are collected.

	Next, the resources themselves are cleaned (as appropriate) in the
	same way.

	Next, an optional post processing stage is called.

	Finally, the page contents and resources in the documents page tree
	are replaced by these processed versions.

	Annotations remain unaffected.

	page: A page loaded by pdf_load_page.

	cookie: A pointer to an optional fz_cookie structure that can be used
	to track progress, collect errors etc.
*/
void pdf_clean_page_contents(fz_context *ctx, pdf_document *doc, pdf_page *page, fz_cookie *cookie, pdf_page_contents_process_fn *proc_fn, void *arg, int sanitize, int ascii)
{
	pdf_filter_page_contents(ctx, doc, page, cookie, proc_fn, NULL, NULL, arg, sanitize, ascii);
}

/*
	Performs the same task as
	pdf_clean_page_contents, but with an optional text filter
	function.

	text_filter: Function to assess whether a given character
	should be kept (return 0) or removed (return 1).

	after_text: Function called after each text object is closed
	to allow other output to be sent.

	arg: Opaque value to be passed to callback functions.
*/
void pdf_filter_page_contents(fz_context *ctx, pdf_document *doc, pdf_page *page, fz_cookie *cookie,
		pdf_page_contents_process_fn *proc_fn, pdf_text_filter_fn *text_filter, pdf_after_text_object_fn *after_text, void *proc_arg,
		int sanitize, int ascii)
{
	pdf_processor *proc_buffer = NULL;
	pdf_processor *proc_filter = NULL;
	pdf_obj *new_obj = NULL;
	pdf_obj *new_ref = NULL;
	pdf_obj *res = NULL;
	pdf_obj *obj;
	pdf_obj *contents;
	pdf_obj *resources;
	fz_buffer *buffer;

	fz_var(new_obj);
	fz_var(new_ref);
	fz_var(res);
	fz_var(proc_buffer);
	fz_var(proc_filter);

	buffer = fz_new_buffer(ctx, 1024);

	fz_try(ctx)
	{
		pdf_obj *sp = pdf_dict_get(ctx, page->obj, PDF_NAME(StructParents));
		int structparents = -1;
		if (pdf_is_number(ctx, sp))
			structparents = pdf_to_int(ctx, sp);
		contents = pdf_page_contents(ctx, page);
		resources = pdf_page_resources(ctx, page);

		proc_buffer = pdf_new_buffer_processor(ctx, buffer, ascii);
		if (sanitize)
		{
			res = pdf_new_dict(ctx, doc, 1);
			proc_filter = pdf_new_filter_processor_with_text_filter(ctx, doc, structparents, proc_buffer, resources, res, text_filter, after_text, proc_arg);
			pdf_process_contents(ctx, proc_filter, doc, resources, contents, cookie);
			pdf_close_processor(ctx, proc_filter);
		}
		else
		{
			res = pdf_keep_obj(ctx, resources);
			pdf_process_contents(ctx, proc_buffer, doc, resources, contents, cookie);
		}
		pdf_close_processor(ctx, proc_buffer);

		/* Deal with page content stream. */

		if (pdf_is_array(ctx, contents))
		{
			/* create a new object to replace the array */
			new_obj = pdf_new_dict(ctx, doc, 1);
			new_ref = pdf_add_object(ctx, doc, new_obj);
			contents = new_ref;
			pdf_dict_put(ctx, page->obj, PDF_NAME(Contents), contents);
		}
		else
		{
			pdf_dict_del(ctx, contents, PDF_NAME(Filter));
			pdf_dict_del(ctx, contents, PDF_NAME(DecodeParms));
		}

		pdf_update_stream(ctx, doc, contents, buffer, 0);

		/* Now deal with resources. The spec allows for Type3 fonts and form
		 * XObjects to omit a resource dictionary and look in the parent.
		 * Avoid that by flattening here as part of the cleaning. This could
		 * conceivably cause changes in rendering, but we don't care. */

		/* ExtGState */
		obj = pdf_dict_get(ctx, res, PDF_NAME(ExtGState));
		if (obj)
		{
			int i, l;

			l = pdf_dict_len(ctx, obj);
			for (i = 0; i < l; i++)
			{
				pdf_obj *o = pdf_dict_get(ctx, pdf_dict_get_val(ctx, obj, i), PDF_NAME(SMask));
				if (!o)
					continue;
				o = pdf_dict_get(ctx, o, PDF_NAME(G));
				if (!o)
					continue;
				/* Transparency group XObject */
				pdf_clean_stream_object(ctx, doc, o, resources, cookie, 1, text_filter, after_text, proc_arg, sanitize, ascii);
			}
		}

		/* Pattern */
		obj = pdf_dict_get(ctx, res, PDF_NAME(Pattern));
		if (obj)
		{
			int i, l;
			l = pdf_dict_len(ctx, obj);
			for (i = 0; i < l; i++)
			{
				pdf_obj *pat_res;
				pdf_obj *pat = pdf_dict_get_val(ctx, obj, i);
				if (!pat)
					continue;
				pat_res = pdf_dict_get(ctx, pat, PDF_NAME(Resources));
				if (pat_res == NULL)
					pat_res = resources;
				if (pdf_dict_get_int(ctx, pat, PDF_NAME(PatternType)) == 1)
					pdf_clean_stream_object(ctx, doc, pat, pat_res, cookie, 0, text_filter, after_text, proc_arg, sanitize, ascii);
			}
		}

		/* XObject */
		obj = pdf_dict_get(ctx, res, PDF_NAME(XObject));
		if (obj)
		{
			int i, l;
			l = pdf_dict_len(ctx, obj);
			for (i = 0; i < l; i++)
			{
				pdf_obj *xobj_res;
				pdf_obj *xobj = pdf_dict_get_val(ctx, obj, i);
				if (!xobj)
					continue;
				xobj_res = pdf_dict_get(ctx, xobj, PDF_NAME(Resources));
				if (xobj_res == NULL)
					xobj_res = resources;
				if (pdf_name_eq(ctx, PDF_NAME(Form), pdf_dict_get(ctx, xobj, PDF_NAME(Subtype))))
					pdf_clean_stream_object(ctx, doc, xobj, xobj_res, cookie, 1, text_filter, after_text, proc_arg, sanitize, ascii);
			}
		}

		/* Font */
		obj = pdf_dict_get(ctx, res, PDF_NAME(Font));
		if (obj)
		{
			int i, l;
			l = pdf_dict_len(ctx, obj);
			for (i = 0; i < l; i++)
			{
				pdf_obj *o = pdf_dict_get_val(ctx, obj, i);
				if (!o)
					continue;
				if (pdf_name_eq(ctx, PDF_NAME(Type3), pdf_dict_get(ctx, o, PDF_NAME(Subtype))))
					pdf_clean_type3(ctx, doc, o, resources, cookie, sanitize, ascii);
			}
		}

		/* ProcSet - no cleaning possible. Inherit this from the old dict. */
		obj = pdf_dict_get(ctx, resources, PDF_NAME(ProcSet));
		if (obj)
			pdf_dict_put(ctx, res, PDF_NAME(ProcSet), obj);

		/* ColorSpace - no cleaning possible. */
		/* Properties - no cleaning possible. */

		if (proc_fn)
			(*proc_fn)(ctx, buffer, res, proc_arg);

		/* Update resource dictionary */
		if (sanitize)
		{
			pdf_dict_put(ctx, page->obj, PDF_NAME(Resources), res);
		}
	}
	fz_always(ctx)
	{
		pdf_drop_processor(ctx, proc_filter);
		pdf_drop_processor(ctx, proc_buffer);
		fz_drop_buffer(ctx, buffer);
		pdf_drop_obj(ctx, new_obj);
		pdf_drop_obj(ctx, new_ref);
		pdf_drop_obj(ctx, res);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

/*
	Clean a loaded annotations rendering operations,
	with an optional post processing step.

	Each appearance stream in the annotation is processed.

	Firstly, this filters the PDF operators used to avoid (some cases
	of) repetition, and leaves the page in a balanced state with an
	unchanged top level matrix etc. At the same time, the resources
	used by the page contents are collected.

	Next, the resources themselves are cleaned (as appropriate) in the
	same way.

	Next, an optional post processing stage is called.

	Finally, the updated stream of operations is reinserted into the
	appearance stream.

	annot: An annotation loaded by pdf_load_annot.

	cookie: A pointer to an optional fz_cookie structure that can be used
	to track progress, collect errors etc.
*/
void pdf_clean_annot_contents(fz_context *ctx, pdf_document *doc, pdf_annot *annot, fz_cookie *cookie, pdf_page_contents_process_fn *proc_fn, void *proc_arg, int sanitize, int ascii)
{
	pdf_filter_annot_contents(ctx, doc, annot, cookie, proc_fn, NULL, NULL, proc_arg, sanitize, ascii);
}

/*
	Performs the same task as
	pdf_clean_annot_contents, but with an optional text filter
	function.

	text_filter: Function to assess whether a given character
	should be kept (return 0) or removed (return 1).

	after_text: Function called after each text object is closed
	to allow other output to be sent.

	arg: Opaque value to be passed to callback functions.
*/
void pdf_filter_annot_contents(fz_context *ctx, pdf_document *doc, pdf_annot *annot, fz_cookie *cookie,
	pdf_page_contents_process_fn *proc, pdf_text_filter_fn *text_filter, pdf_after_text_object_fn *after_text, void *arg, int sanitize, int ascii)
{
	pdf_obj *ap;
	int i, n;

	ap = pdf_dict_get(ctx, annot->obj, PDF_NAME(AP));
	if (ap == NULL)
		return;

	n = pdf_dict_len(ctx, ap);
	for (i = 0; i < n; i++)
	{
		pdf_obj *v = pdf_dict_get_val(ctx, ap, i);

		if (v == NULL)
			continue;

		pdf_clean_stream_object(ctx, doc, v, NULL, cookie, 1, text_filter, after_text, arg, sanitize, ascii);
	}
}

static void
pdf_redact_end_page(fz_context *ctx, fz_buffer *buf, pdf_obj *res, void *opaque)
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
	fz_rect r;
	fz_quad q;
	int i, n;

	trm = fz_concat(trm, ctm);

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
					if (fz_is_point_inside_quad(fz_make_point(trm.e, trm.f), q))
						return 1;
				}
			}
			else
			{
				r = pdf_dict_get_rect(ctx, annot->obj, PDF_NAME(Rect));
				if (fz_is_point_inside_rect(fz_make_point(trm.e, trm.f), r))
					return 1;
			}
		}
	}

	return 0;
}

int
pdf_redact_page(fz_context *ctx, pdf_document *doc, pdf_page *page, pdf_redact_options *opts)
{
	pdf_annot *annot;
	int has_redactions = 0;
	int no_black_boxes = 0;

	if (opts)
	{
		no_black_boxes = opts->no_black_boxes;
	}

	for (annot = pdf_first_annot(ctx, page); annot; annot = pdf_next_annot(ctx, annot))
		if (pdf_dict_get(ctx, annot->obj, PDF_NAME(Subtype)) == PDF_NAME(Redact))
			has_redactions = 1;

	if (has_redactions)
	{
		pdf_filter_page_contents(ctx, doc, page, NULL,
			no_black_boxes ? NULL : pdf_redact_end_page,
			pdf_redact_text_filter,
			NULL,
			page,
			1, 1);
	}

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

	doc->redacted = has_redactions;

	return has_redactions;
}
