#include "pdf-interpret-imp.h"

static void
pdf_clean_stream_object(pdf_document *doc, pdf_obj *obj, pdf_obj *orig_res, fz_cookie *cookie, int own_res)
{
	fz_context *ctx = doc->ctx;
	pdf_process process, process2;
	fz_buffer *buffer;
	int num;
	pdf_obj *res = NULL;
	pdf_obj *ref = NULL;

	if (!obj)
		return;

	fz_var(res);
	fz_var(ref);

	buffer = fz_new_buffer(ctx, 1024);

	fz_try(ctx)
	{
		if (own_res)
		{
			pdf_obj *r = pdf_dict_gets(obj, "Resources");
			if (r)
				orig_res = r;
		}

		res = pdf_new_dict(doc, 1);

		pdf_process_buffer(&process2, ctx, buffer);
		pdf_process_filter(&process, ctx, &process2, res);

		pdf_process_stream_object(doc, obj, &process, orig_res, cookie);

		num = pdf_to_num(obj);
		pdf_dict_dels(obj, "Filter");
		pdf_update_stream(doc, num, buffer);

		if (own_res)
		{
			ref = pdf_new_ref(doc, res);
			pdf_dict_puts(obj, "Resources", ref);
		}
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buffer);
		pdf_drop_obj(res);
		pdf_drop_obj(ref);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "Failed while cleaning xobject");
	}
}

static void
pdf_clean_type3(pdf_document *doc, pdf_obj *obj, pdf_obj *orig_res, fz_cookie *cookie)
{
	fz_context *ctx = doc->ctx;
	pdf_process process, process2;
	fz_buffer *buffer;
	int num, i, l;
	pdf_obj *res = NULL;
	pdf_obj *ref = NULL;
	pdf_obj *charprocs;

	fz_var(res);
	fz_var(ref);

	fz_try(ctx)
	{
		res = pdf_dict_gets(obj, "Resources");
		if (res)
			orig_res = res;
		res = NULL;

		res = pdf_new_dict(doc, 1);

		charprocs = pdf_dict_gets(obj, "CharProcs");
		l = pdf_dict_len(charprocs);

		for (i = 0; i < l; i++)
		{
			pdf_obj *key = pdf_dict_get_key(charprocs, i);
			pdf_obj *val = pdf_dict_get_val(charprocs, i);

			buffer = fz_new_buffer(ctx, 1024);
			pdf_process_buffer(&process2, ctx, buffer);
			pdf_process_filter(&process, ctx, &process2, res);

			pdf_process_stream_object(doc, val, &process, orig_res, cookie);

			num = pdf_to_num(val);
			pdf_dict_dels(val, "Filter");
			pdf_update_stream(doc, num, buffer);
			pdf_dict_put(charprocs, key, val);
			fz_drop_buffer(ctx, buffer);
			buffer = NULL;
		}

		/* ProcSet - no cleaning possible. Inherit this from the old dict. */
		pdf_dict_puts(res, "ProcSet", pdf_dict_gets(orig_res, "ProcSet"));

		ref = pdf_new_ref(doc, res);
		pdf_dict_puts(obj, "Resources", ref);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buffer);
		pdf_drop_obj(res);
		pdf_drop_obj(ref);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "Failed while cleaning xobject");
	}
}

void pdf_clean_page_contents(pdf_document *doc, pdf_page *page, fz_cookie *cookie)
{
	fz_context *ctx = doc->ctx;
	pdf_process process, process2;
	fz_buffer *buffer = fz_new_buffer(ctx, 1024);
	int num;
	pdf_obj *contents;
	pdf_obj *new_obj = NULL;
	pdf_obj *new_ref = NULL;
	pdf_obj *res = NULL;
	pdf_obj *ref = NULL;
	pdf_obj *obj;

	fz_var(new_obj);
	fz_var(new_ref);
	fz_var(res);
	fz_var(ref);

	fz_try(ctx)
	{
		res = pdf_new_dict(doc, 1);

		pdf_process_buffer(&process2, ctx, buffer);
		pdf_process_filter(&process, ctx, &process2, res);

		pdf_process_stream_object(doc, page->contents, &process, page->resources, cookie);

		contents = page->contents;
		if (pdf_is_array(contents))
		{
			int n = pdf_array_len(contents);
			int i;

			for (i = n-1; i > 0; i--)
				pdf_array_delete(contents, i);
			/* We cannot rewrite the 0th entry of contents
			 * directly as it may occur in other pages content
			 * dictionaries too. We therefore clone it and make
			 * a new object reference. */
			new_obj = pdf_copy_dict(pdf_array_get(contents, 0));
			new_ref = pdf_new_ref(doc, new_obj);
			num = pdf_to_num(new_ref);
			pdf_array_put(contents, 0, new_ref);
			pdf_dict_dels(new_obj, "Filter");
		}
		else
		{
			num = pdf_to_num(contents);
			pdf_dict_dels(contents, "Filter");
		}
		pdf_update_stream(doc, num, buffer);

		/* Now deal with resources. The spec allows for Type3 fonts and form
		 * XObjects to omit a resource dictionary and look in the parent.
		 * Avoid that by flattening here as part of the cleaning. This could
		 * conceivably cause changes in rendering, but we don't care. */

		/* ExtGState */
		obj = pdf_dict_gets(res, "ExtGState");
		if (obj)
		{
			int i, l;

			l = pdf_dict_len(obj);
			for (i = 0; i < l; i++)
			{
				pdf_obj *o = pdf_dict_gets(pdf_dict_get_val(obj, i), "SMask");

				if (!o)
					continue;
				o = pdf_dict_gets(o, "G");
				if (!o)
					continue;

				/* Transparency group XObject */
				pdf_clean_stream_object(doc, o, page->resources, cookie, 1);
			}
		}

		/* ColorSpace - no cleaning possible */

		/* Pattern */
		obj = pdf_dict_gets(res, "Pattern");
		if (obj)
		{
			int i, l;

			l = pdf_dict_len(obj);
			for (i = 0; i < l; i++)
			{
				pdf_obj *pat = pdf_dict_get_val(obj, i);

				if (!pat)
					continue;
				if (pdf_to_int(pdf_dict_gets(pat, "PatternType")) == 1)
					pdf_clean_stream_object(doc, pat, page->resources, cookie, 0);
			}
		}

		/* Shading - no cleaning possible */

		/* XObject */
		obj = pdf_dict_gets(res, "XObject");
		if (obj)
		{
			int i, l;

			l = pdf_dict_len(obj);
			for (i = 0; i < l; i++)
			{
				pdf_obj *xobj = pdf_dict_get_val(obj, i);

				if (strcmp(pdf_to_name(pdf_dict_gets(xobj, "Subtype")), "Form"))
					continue;

				pdf_clean_stream_object(doc, xobj, page->resources, cookie, 1);
			}
		}

		/* Font */
		obj = pdf_dict_gets(res, "Font");
		if (obj)
		{
			int i, l;

			l = pdf_dict_len(obj);
			for (i = 0; i < l; i++)
			{
				pdf_obj *o = pdf_dict_get_val(obj, i);

				if (!strcmp(pdf_to_name(pdf_dict_gets(o, "Subtype")), "Type3"))
				{
					pdf_clean_type3(doc, o, page->resources, cookie);
				}
			}
		}

		/* ProcSet - no cleaning possible. Inherit this from the old dict. */
		obj = pdf_dict_gets(page->resources, "ProcSet");
		if (obj)
			pdf_dict_puts(res, "ProcSet", obj);

		/* Properties - no cleaning possible. */

		pdf_drop_obj(page->resources);
		ref = pdf_new_ref(doc, res);
		page->resources = pdf_keep_obj(ref);
		pdf_dict_puts(page->me, "Resources", ref);
	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buffer);
		pdf_drop_obj(new_obj);
		pdf_drop_obj(new_ref);
		pdf_drop_obj(res);
		pdf_drop_obj(ref);
	}
	fz_catch(ctx)
	{
		fz_rethrow_message(ctx, "Failed while cleaning page");
	}
}
