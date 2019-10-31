#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "../fitz/fitz-imp.h"

#include <string.h>

void pdf_write_digest(fz_context *ctx, fz_output *out, pdf_obj *byte_range, int hexdigest_offset, int hexdigest_length, pdf_pkcs7_signer *signer)
{
	fz_stream *stm = NULL;
	fz_stream *in = NULL;
	fz_range *brange = NULL;
	int brange_len = pdf_array_len(ctx, byte_range)/2;
	unsigned char *digest = NULL;
	int digest_len;

	fz_var(stm);
	fz_var(in);
	fz_var(brange);
	fz_var(digest);

	if (hexdigest_length < 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Bad parameters to pdf_write_digest");

	fz_try(ctx)
	{
		int i, res;

		brange = fz_calloc(ctx, brange_len, sizeof(*brange));
		for (i = 0; i < brange_len; i++)
		{
			brange[i].offset = pdf_array_get_int(ctx, byte_range, 2*i);
			brange[i].length = pdf_array_get_int(ctx, byte_range, 2*i+1);
		}

		stm = fz_stream_from_output(ctx, out);
		in = fz_open_range_filter(ctx, stm, brange, brange_len);

		digest_len = (hexdigest_length - 2) / 2;
		digest = fz_malloc(ctx, digest_len);
		res = signer->create_digest(signer, in, digest, &digest_len);
		if (!res)
			fz_throw(ctx, FZ_ERROR_GENERIC, "pdf_pkcs7_create_digest failed");

		fz_drop_stream(ctx, in);
		in = NULL;
		fz_drop_stream(ctx, stm);
		stm = NULL;

		fz_seek_output(ctx, out, hexdigest_offset+1, SEEK_SET);

		for (i = 0; i < digest_len; i++)
			fz_write_printf(ctx, out, "%02x", digest[i]);
	}
	fz_always(ctx)
	{
		fz_free(ctx, digest);
		fz_free(ctx, brange);
		fz_drop_stream(ctx, stm);
		fz_drop_stream(ctx, in);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_sign_signature(fz_context *ctx, pdf_document *doc, pdf_widget *widget, pdf_pkcs7_signer *signer)
{
	pdf_pkcs7_designated_name *dn = NULL;
	fz_buffer *fzbuf = NULL;

	fz_try(ctx)
	{
		const char *dn_str;
		pdf_obj *wobj = ((pdf_annot *)widget)->obj;
		fz_rect rect;

		rect = pdf_dict_get_rect(ctx, wobj, PDF_NAME(Rect));

		/* Create an appearance stream only if the signature is intended to be visible */
		if (!fz_is_empty_rect(rect))
		{
			dn = signer->designated_name(signer);
			fzbuf = fz_new_buffer(ctx, 256);
			if (!dn->cn)
				fz_throw(ctx, FZ_ERROR_GENERIC, "Certificate has no common name");

			fz_append_printf(ctx, fzbuf, "cn=%s", dn->cn);

			if (dn->o)
				fz_append_printf(ctx, fzbuf, ", o=%s", dn->o);

			if (dn->ou)
				fz_append_printf(ctx, fzbuf, ", ou=%s", dn->ou);

			if (dn->email)
				fz_append_printf(ctx, fzbuf, ", email=%s", dn->email);

			if (dn->c)
				fz_append_printf(ctx, fzbuf, ", c=%s", dn->c);

			dn_str = fz_string_from_buffer(ctx, fzbuf);
			pdf_update_signature_appearance(ctx, (pdf_annot *)widget, dn->cn, dn_str, NULL);
		}

		pdf_signature_set_value(ctx, doc, wobj, signer);
	}
	fz_always(ctx)
	{
		signer->drop_designated_name(signer, dn);
		fz_drop_buffer(ctx, fzbuf);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_clear_signature(fz_context *ctx, pdf_document *doc, pdf_widget *widget)
{
	int flags;

	flags = pdf_dict_get_int(ctx, ((pdf_annot *) widget)->obj, PDF_NAME(F));
	flags &= ~PDF_ANNOT_IS_LOCKED;
	if (flags)
		pdf_dict_put_int(ctx, ((pdf_annot *) widget)->obj, PDF_NAME(F), flags);
	else
		pdf_dict_del(ctx, ((pdf_annot *) widget)->obj, PDF_NAME(F));

	pdf_dict_del(ctx, ((pdf_annot *) widget)->obj, PDF_NAME(V));

	pdf_update_signature_appearance(ctx, widget, NULL, NULL, NULL);
}
