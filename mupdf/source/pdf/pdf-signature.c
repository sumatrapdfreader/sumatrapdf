#include "mupdf/fitz.h"
#include "mupdf/pdf.h"
#include "../fitz/fitz-imp.h"

#include <string.h>
#include <time.h>

enum
{
	PDF_SIGFLAGS_SIGSEXIST = 1,
	PDF_SIGFLAGS_APPENDONLY = 2
};

void pdf_write_digest(fz_context *ctx, fz_output *out, pdf_obj *byte_range, size_t hexdigest_offset, size_t hexdigest_length, pdf_pkcs7_signer *signer)
{
	fz_stream *stm = NULL;
	fz_stream *in = NULL;
	fz_range *brange = NULL;
	int brange_len = pdf_array_len(ctx, byte_range)/2;
	unsigned char *digest = NULL;
	size_t digest_len;

	fz_var(stm);
	fz_var(in);
	fz_var(brange);
	fz_var(digest);

	if (hexdigest_length < 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Bad parameters to pdf_write_digest");

	fz_try(ctx)
	{
		int i, res;
		size_t z;

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

		fz_seek_output(ctx, out, (int64_t)hexdigest_offset+1, SEEK_SET);

		for (z = 0; z < digest_len; z++)
			fz_write_printf(ctx, out, "%02x", digest[z]);
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

typedef struct fieldname_prefix_s
{
	struct fieldname_prefix_s *prev;
	char name[1];
} fieldname_prefix;

typedef struct
{
	pdf_locked_fields *locked;
	fieldname_prefix *prefix;
} sig_locking_data;

static void
check_field_locking(fz_context *ctx, pdf_obj *obj, void *data_, pdf_obj **ff)
{
	fieldname_prefix *prefix = NULL;
	sig_locking_data *data = (sig_locking_data *)data_;

	fz_var(prefix);

	fz_try(ctx)
	{
		const char *name;
		size_t n;

		name = pdf_to_text_string(ctx, pdf_dict_get(ctx, obj, PDF_NAME(T)));
		n = strlen(name)+1;
		if (data->prefix->name[0])
			n += 1 + strlen(data->prefix->name);
		prefix = fz_calloc(ctx, 1, sizeof(*prefix)+n);
		prefix->prev = data->prefix;
		if (data->prefix->name[0])
		{
			strcpy(prefix->name, data->prefix->name);
			strcat(prefix->name, ".");
		}
		else
			prefix->name[0] = 0;
		strcat(prefix->name, name);
		data->prefix = prefix;

		if (pdf_name_eq(ctx, pdf_dict_get(ctx, obj, PDF_NAME(Type)), PDF_NAME(Annot)) &&
			pdf_name_eq(ctx, pdf_dict_get(ctx, obj, PDF_NAME(Subtype)), PDF_NAME(Widget)))
		{
			int flags = pdf_to_int(ctx, *ff);

			if (((flags & PDF_FIELD_IS_READ_ONLY) == 0) && /* Field is not currently locked */
				pdf_is_field_locked(ctx, data->locked, data->prefix->name)) /* Field should be locked */
				pdf_dict_put_drop(ctx, obj, PDF_NAME(Ff), pdf_new_int(ctx, flags | PDF_FIELD_IS_READ_ONLY));
		}
	}
	fz_catch(ctx)
	{
		if (prefix)
		{
			data->prefix = prefix->prev;
			fz_free(ctx, prefix);
		}
		fz_rethrow(ctx);
	}
}

static void
pop_field_locking(fz_context *ctx, pdf_obj *obj, void *data_)
{
	fieldname_prefix *prefix;
	sig_locking_data *data = (sig_locking_data *)data_;

	prefix = data->prefix;
	data->prefix = data->prefix->prev;
	fz_free(ctx, prefix);
}

static void enact_sig_locking(fz_context *ctx, pdf_document *doc, pdf_obj *sig)
{
	pdf_locked_fields *locked = pdf_find_locked_fields_for_sig(ctx, doc, sig);
	pdf_obj *fields;
	static pdf_obj *ff_names[2] = { PDF_NAME(Ff), NULL };
	pdf_obj *ff = NULL;
	static fieldname_prefix null_prefix = { NULL, "" };
	sig_locking_data data = { locked, &null_prefix };

	if (locked == NULL)
		return;

	fields = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/Fields");
	pdf_walk_tree(ctx, fields, PDF_NAME(Kids), check_field_locking, pop_field_locking, &data, &ff_names[0], &ff);
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
		time_t now = time(NULL);
#ifdef _POSIX_SOURCE
		struct tm tmbuf, *tm = gmtime_r(&now, &tmbuf);
#else
		struct tm *tm = gmtime(&now);
#endif
		char now_str[40];
		size_t len = 0;
		pdf_obj *form;
		int sf;

		/* Ensure that all fields that will be locked by this signature
		 * are marked as ReadOnly. */
		enact_sig_locking(ctx, doc, wobj);

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

			if (tm)
				len = strftime(now_str, sizeof now_str, "%Y.%m.%d %H:%M:%SZ", tm);
			pdf_update_signature_appearance(ctx, (pdf_annot *)widget, dn->cn, dn_str, len?now_str:NULL);
		}

		pdf_signature_set_value(ctx, doc, wobj, signer, now);

		/* Update the SigFlags for the document if required */
		form = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm");
		sf = pdf_to_int(ctx, pdf_dict_get(ctx, form, PDF_NAME(SigFlags)));
		if ((sf & (PDF_SIGFLAGS_SIGSEXIST | PDF_SIGFLAGS_APPENDONLY)) != (PDF_SIGFLAGS_SIGSEXIST | PDF_SIGFLAGS_APPENDONLY))
			pdf_dict_put_drop(ctx, form, PDF_NAME(SigFlags), pdf_new_int(ctx, sf | PDF_SIGFLAGS_SIGSEXIST | PDF_SIGFLAGS_APPENDONLY));
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
