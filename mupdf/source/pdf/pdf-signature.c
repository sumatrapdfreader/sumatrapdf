#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

#include <string.h>
#include <time.h>

enum
{
	PDF_SIGFLAGS_SIGSEXIST = 1,
	PDF_SIGFLAGS_APPENDONLY = 2
};

void pdf_write_digest(fz_context *ctx, fz_output *out, pdf_obj *byte_range, pdf_obj *field, size_t hexdigest_offset, size_t hexdigest_length, pdf_pkcs7_signer *signer)
{
	fz_stream *stm = NULL;
	fz_stream *in = NULL;
	fz_range *brange = NULL;
	int brange_len = pdf_array_len(ctx, byte_range)/2;
	unsigned char *digest = NULL;
	size_t digest_len;
	pdf_obj *v = pdf_dict_get(ctx, field, PDF_NAME(V));
	size_t len;
	char *cstr = NULL;

	fz_var(stm);
	fz_var(in);
	fz_var(brange);
	fz_var(digest);
	fz_var(cstr);

	if (hexdigest_length < 4)
		fz_throw(ctx, FZ_ERROR_GENERIC, "Bad parameters to pdf_write_digest");

	len = (hexdigest_length - 2) / 2;

	fz_try(ctx)
	{
		int i;
		size_t z;

		brange = fz_calloc(ctx, brange_len, sizeof(*brange));
		for (i = 0; i < brange_len; i++)
		{
			brange[i].offset = pdf_array_get_int(ctx, byte_range, 2*i);
			brange[i].length = pdf_array_get_int(ctx, byte_range, 2*i+1);
		}

		stm = fz_stream_from_output(ctx, out);
		in = fz_open_range_filter(ctx, stm, brange, brange_len);

		digest = fz_malloc(ctx, len);
		digest_len = signer->create_digest(ctx, signer, in, digest, len);
		if (digest_len == 0)
			fz_throw(ctx, FZ_ERROR_GENERIC, "signer provided no signature digest");
		if (digest_len > len)
			fz_throw(ctx, FZ_ERROR_GENERIC, "signature digest larger than space for digest");

		fz_drop_stream(ctx, in);
		in = NULL;
		fz_drop_stream(ctx, stm);
		stm = NULL;

		fz_seek_output(ctx, out, (int64_t)hexdigest_offset+1, SEEK_SET);
		cstr = fz_malloc(ctx, len);

		for (z = 0; z < len; z++)
		{
			int val = z < digest_len ? digest[z] : 0;
			fz_write_printf(ctx, out, "%02x", val);
			cstr[z] = val;
		}

		pdf_dict_put_string(ctx, v, PDF_NAME(Contents), cstr, len);
	}
	fz_always(ctx)
	{
		fz_free(ctx, cstr);
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

typedef struct fieldname_prefix
{
	struct fieldname_prefix *prev;
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
		const char *name = NULL;
		size_t n = 1;
		pdf_obj *t;

		t = pdf_dict_get(ctx, obj, PDF_NAME(T));
		if (t != NULL)
		{
			name = pdf_to_text_string(ctx, t);
			n += strlen(name);
		}
		if (data->prefix->name[0] && name)
			n += 1;
		if (data->prefix->name[0])
			n += strlen(data->prefix->name);
		prefix = fz_calloc(ctx, 1, sizeof(*prefix)+n);
		prefix->prev = data->prefix;
		if (data->prefix->name[0])
			strcpy(prefix->name, data->prefix->name);
		if (data->prefix->name[0] && name)
			strcat(prefix->name, ".");
		if (name)
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

	fz_try(ctx)
	{
		fields = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm/Fields");
		pdf_walk_tree(ctx, fields, PDF_NAME(Kids), check_field_locking, pop_field_locking, &data, &ff_names[0], &ff);
	}
	fz_always(ctx)
		pdf_drop_locked_fields(ctx, locked);
	fz_catch(ctx)
		fz_rethrow(ctx);
}

void pdf_sign_signature(fz_context *ctx, pdf_widget *widget, pdf_pkcs7_signer *signer, fz_image *image)
{
	pdf_pkcs7_designated_name *dn = NULL;
	fz_buffer *fzbuf = NULL;
	pdf_document *doc = widget->page->doc;

	if (pdf_widget_is_readonly(ctx, widget))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Signature is read only, it cannot be signed.");

	fz_var(dn);
	fz_var(fzbuf);

	pdf_begin_operation(ctx, doc, "Sign signature");

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
#ifdef CLUSTER
		memset(&now, 0, sizeof(now));
		memset(tm, 0, sizeof(*tm));
#endif

		pdf_dirty_annot(ctx, widget);

		/* Ensure that all fields that will be locked by this signature
		 * are marked as ReadOnly. */
		enact_sig_locking(ctx, doc, wobj);

		rect = pdf_dict_get_rect(ctx, wobj, PDF_NAME(Rect));

		/* Create an appearance stream only if the signature is intended to be visible */
		if (!fz_is_empty_rect(rect))
		{
			if (image)
			{
				pdf_update_signature_appearance_with_image(ctx, (pdf_annot *)widget, image);
			}
			else
			{
				dn = signer->get_signing_name(ctx, signer);
				if (!dn || !dn->cn)
					fz_throw(ctx, FZ_ERROR_GENERIC, "Certificate has no common name");

				fzbuf = fz_new_buffer(ctx, 256);
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
		}

		/* Update the SigFlags for the document if required */
		form = pdf_dict_getp(ctx, pdf_trailer(ctx, doc), "Root/AcroForm");
		if (!form)
		{
			pdf_obj *root = pdf_dict_get(ctx, pdf_trailer(ctx, doc), PDF_NAME(Root));
			form = pdf_dict_put_dict(ctx, root, PDF_NAME(AcroForm), 1);
		}

		sf = pdf_to_int(ctx, pdf_dict_get(ctx, form, PDF_NAME(SigFlags)));
		if ((sf & (PDF_SIGFLAGS_SIGSEXIST | PDF_SIGFLAGS_APPENDONLY)) != (PDF_SIGFLAGS_SIGSEXIST | PDF_SIGFLAGS_APPENDONLY))
			pdf_dict_put_drop(ctx, form, PDF_NAME(SigFlags), pdf_new_int(ctx, sf | PDF_SIGFLAGS_SIGSEXIST | PDF_SIGFLAGS_APPENDONLY));

		pdf_signature_set_value(ctx, doc, wobj, signer, now);
	}
	fz_always(ctx)
	{
		pdf_end_operation(ctx, doc);
		fz_drop_buffer(ctx, fzbuf);
		pdf_signature_drop_designated_name(ctx, dn);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}

void pdf_clear_signature(fz_context *ctx, pdf_widget *widget)
{
	int flags;

	if (pdf_widget_is_readonly(ctx, widget))
		fz_throw(ctx, FZ_ERROR_GENERIC, "Signature read only, it cannot be cleared.");

	pdf_xref_remove_unsaved_signature(ctx, ((pdf_annot *) widget)->page->doc,  ((pdf_annot *) widget)->obj);

	pdf_dirty_annot(ctx, widget);

	flags = pdf_dict_get_int(ctx, ((pdf_annot *) widget)->obj, PDF_NAME(F));
	flags &= ~PDF_ANNOT_IS_LOCKED;
	if (flags)
		pdf_dict_put_int(ctx, ((pdf_annot *) widget)->obj, PDF_NAME(F), flags);
	else
		pdf_dict_del(ctx, ((pdf_annot *) widget)->obj, PDF_NAME(F));

	pdf_dict_del(ctx, ((pdf_annot *) widget)->obj, PDF_NAME(V));

	pdf_update_signature_appearance(ctx, widget, NULL, NULL, NULL);
}

void pdf_drop_signer(fz_context *ctx, pdf_pkcs7_signer *signer)
{
	if (signer)
		signer->drop(ctx, signer);
}

void pdf_drop_verifier(fz_context *ctx, pdf_pkcs7_verifier *verifier)
{
	if (verifier)
		verifier->drop(ctx, verifier);
}

char *pdf_signature_error_description(pdf_signature_error err)
{
	switch (err)
	{
	case PDF_SIGNATURE_ERROR_OKAY:
		return "OK";
	case PDF_SIGNATURE_ERROR_NO_SIGNATURES:
		return "No signatures.";
	case PDF_SIGNATURE_ERROR_NO_CERTIFICATE:
		return "No certificate.";
	case PDF_SIGNATURE_ERROR_DIGEST_FAILURE:
		return "Signature invalidated by change to document.";
	case PDF_SIGNATURE_ERROR_SELF_SIGNED:
		return "Self-signed certificate.";
	case PDF_SIGNATURE_ERROR_SELF_SIGNED_IN_CHAIN:
		return "Self-signed certificate in chain.";
	case PDF_SIGNATURE_ERROR_NOT_TRUSTED:
		return "Certificate not trusted.";
	default:
	case PDF_SIGNATURE_ERROR_UNKNOWN:
		return "Unknown error.";
	}
}

void pdf_signature_drop_designated_name(fz_context *ctx, pdf_pkcs7_designated_name *dn)
{
	if (dn)
	{
		fz_free(ctx, dn->c);
		fz_free(ctx, dn->email);
		fz_free(ctx, dn->ou);
		fz_free(ctx, dn->o);
		fz_free(ctx, dn->cn);
		fz_free(ctx, dn);
	}
}

char *pdf_signature_format_designated_name(fz_context *ctx, pdf_pkcs7_designated_name *name)
{
	const char *parts[] = {
		"CN=", "",
		", O=", "",
		", OU=", "",
		", emailAddress=", "",
		", C=", ""};
	size_t len = 1;
	char *s;
	int i;

	if (name == NULL)
		return NULL;

	parts[1] = name->cn;
	parts[3] = name->o;
	parts[5] = name->ou;
	parts[7] = name->email;
	parts[9] = name->c;

	for (i = 0; i < (int)nelem(parts); i++)
		if (parts[i])
			len += strlen(parts[i]);

	s = fz_malloc(ctx, len);
	s[0] = '\0';

	for (i = 0; i < (int)nelem(parts); i++)
		if (parts[i])
			fz_strlcat(s, parts[i], len);

	return s;
}

pdf_pkcs7_designated_name *pdf_signature_get_signatory(fz_context *ctx, pdf_pkcs7_verifier *verifier, pdf_document *doc, pdf_obj *signature)
{
	char *contents = NULL;
	size_t contents_len;
	pdf_pkcs7_designated_name *dn;

	contents_len = pdf_signature_contents(ctx, doc, signature, &contents);
	if (contents_len == 0)
		return NULL;

	fz_try(ctx)
		dn = verifier->get_signatory(ctx, verifier, (unsigned char *)contents, contents_len);
	fz_always(ctx)
		fz_free(ctx, contents);
	fz_catch(ctx)
		fz_rethrow(ctx);

	return dn;
}

pdf_signature_error pdf_check_digest(fz_context *ctx, pdf_pkcs7_verifier *verifier, pdf_document *doc, pdf_obj *signature)
{
	pdf_signature_error result = PDF_SIGNATURE_ERROR_UNKNOWN;
	fz_stream *bytes = NULL;
	char *contents = NULL;
	size_t contents_len = pdf_signature_contents(ctx, doc, signature, &contents);
	fz_var(bytes);
	fz_try(ctx)
	{
		bytes = pdf_signature_hash_bytes(ctx, doc, signature);
		result = verifier->check_digest(ctx, verifier, bytes, (unsigned char *)contents, contents_len);
	}
	fz_always(ctx)
	{
		fz_drop_stream(ctx, bytes);
		fz_free(ctx, contents);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}

	return result;
}

pdf_signature_error pdf_check_certificate(fz_context *ctx, pdf_pkcs7_verifier *verifier, pdf_document *doc, pdf_obj *signature)
{
	char *contents = NULL;
	size_t contents_len = pdf_signature_contents(ctx, doc, signature, &contents);
	pdf_signature_error result = PDF_SIGNATURE_ERROR_UNKNOWN;
	fz_try(ctx)
		result = verifier->check_certificate(ctx, verifier, (unsigned char *)contents, contents_len);
	fz_always(ctx)
		fz_free(ctx, contents);
	fz_catch(ctx)
		fz_rethrow(ctx);
	return result;
}

int pdf_check_signature(fz_context *ctx, pdf_pkcs7_verifier *verifier, pdf_document *doc, pdf_obj *signature, char *ebuf, size_t ebufsize)
{
	int res = 0;

	if (pdf_xref_obj_is_unsaved_signature(doc, signature))
	{
		fz_strlcpy(ebuf, "Signed but document yet to be saved.", ebufsize);
		if (ebufsize > 0)
			ebuf[ebufsize-1] = 0;
		return 0;
	}

	fz_var(res);
	fz_try(ctx)
	{
		if (pdf_signature_is_signed(ctx, doc, signature))
		{
			pdf_signature_error err;

			err = pdf_check_digest(ctx, verifier, doc, signature);
			if (err == PDF_SIGNATURE_ERROR_OKAY)
				err = pdf_check_certificate(ctx, verifier, doc, signature);

			fz_strlcpy(ebuf, pdf_signature_error_description(err), ebufsize);
			res = (err == PDF_SIGNATURE_ERROR_OKAY);

			switch (err)
			{
			case PDF_SIGNATURE_ERROR_SELF_SIGNED:
			case PDF_SIGNATURE_ERROR_SELF_SIGNED_IN_CHAIN:
			case PDF_SIGNATURE_ERROR_NOT_TRUSTED:
			{
				pdf_pkcs7_designated_name *dn;

				dn = pdf_signature_get_signatory(ctx, verifier, doc, signature);
				if (dn)
				{
					char *s = pdf_signature_format_designated_name(ctx, dn);
					pdf_signature_drop_designated_name(ctx, dn);
					fz_strlcat(ebuf, " (", ebufsize);
					fz_strlcat(ebuf, s, ebufsize);
					fz_free(ctx, s);
				}
				else
				{
					fz_strlcat(ebuf, "()", ebufsize);
				}

				break;
			}
			default:
				break;
			}
		}
		else
		{
			res = 0;
			fz_strlcpy(ebuf, "Not signed.", ebufsize);
		}
	}
	fz_catch(ctx)
	{
		res = 0;
		fz_strlcpy(ebuf, fz_caught_message(ctx), ebufsize);
	}

	if (ebufsize > 0)
		ebuf[ebufsize-1] = 0;

	return res;
}
